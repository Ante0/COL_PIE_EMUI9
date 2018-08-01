/*
 * Copyright (c) 2013
 * Phillip Lougher <phillip@squashfs.org.uk>
 *
 * This work is licensed under the terms of the GNU GPL, version 2. See
 * the COPYING file in the top-level directory.
 */

#include <linux/fs.h>
#include <linux/vfs.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/pagemap.h>
#include <linux/mutex.h>
#include <linux/mm_inline.h>

#include "squashfs_fs.h"
#include "squashfs_fs_sb.h"
#include "squashfs_fs_i.h"
#include "squashfs.h"
#include "page_actor.h"

static void release_actor_pages(struct page **page, int pages, int error)
{
	int i;

	for (i = 0; i < pages; i++) {
		if (!page[i])
			continue;
		flush_dcache_page(page[i]);
		if (!error)
			SetPageUptodate(page[i]);
		else {
			SetPageError(page[i]);
			zero_user_segment(page[i], 0, PAGE_SIZE);
		}
		unlock_page(page[i]);
		put_page(page[i]);
	}
	kfree(page);
}

/*
 * Create a "page actor" which will kmap and kunmap the
 * page cache pages appropriately within the decompressor
 */
static struct squashfs_page_actor *actor_from_page_cache(
	unsigned int actor_pages, struct page *target_page,
	struct list_head *rpages, unsigned int *nr_pages, int start_index,
	struct address_space *mapping)
{
	struct page **page;
	struct squashfs_page_actor *actor;
	int i, n;
	gfp_t gfp = mapping_gfp_constraint(mapping, GFP_KERNEL);

	page = kmalloc_array(actor_pages, sizeof(void *), GFP_KERNEL);
	if (!page)
		return NULL;

	for (i = 0, n = start_index; i < actor_pages; i++, n++) {
		if (target_page == NULL && rpages && !list_empty(rpages)) {
			struct page *cur_page = lru_to_page(rpages);

			if (cur_page->index < start_index + actor_pages) {
				list_del(&cur_page->lru);
				--(*nr_pages);
				if (add_to_page_cache_lru(cur_page, mapping,
							  cur_page->index, gfp))
					put_page(cur_page);
				else
					target_page = cur_page;
			} else
				rpages = NULL;
		}

		if (target_page && target_page->index == n) {
			page[i] = target_page;
			target_page = NULL;
		} else {
			page[i] = grab_cache_page_nowait(mapping, n);
			if (page[i] == NULL)
				continue;
		}

		if (PageUptodate(page[i])) {
			unlock_page(page[i]);
			put_page(page[i]);
			page[i] = NULL;
		}
	}

	actor = squashfs_page_actor_init(page, actor_pages, 0,
			release_actor_pages);
	if (!actor) {
		release_actor_pages(page, actor_pages, -ENOMEM);
		kfree(page);
		return NULL;
	}
	return actor;
}

int squashfs_readpages_block(struct page *target_page,
			     struct list_head *readahead_pages,
			     unsigned int *nr_pages,
			     struct address_space *mapping,
			     int page_index, u64 block, int bsize)

{
	struct inode *i = target_page->mapping->host;
	struct squashfs_cache_entry *buffer = squashfs_get_datablock(i->i_sb,
						 block, bsize);
	int bytes = buffer->length, res = buffer->error, n, offset = 0;

	if (res) {
		ERROR("Unable to read page, block %llx, size %x\n", block,
			bsize);
		goto out;
	}

	for (n = 0; n < pages && bytes > 0; n++,
			bytes -= PAGE_SIZE, offset += PAGE_SIZE) {
		int avail = min_t(int, bytes, PAGE_SIZE);

		if (page[n] == NULL)
			continue;

		squashfs_fill_page(page[n], buffer, offset, avail);
		unlock_page(page[n]);
		if (page[n] != target_page)
			put_page(page[n]);
	}

	actor = actor_from_page_cache(actor_pages, target_page,
				      readahead_pages, nr_pages, start_index,
				      mapping);
	if (!actor)
		return -ENOMEM;

	res = squashfs_read_data_async(inode->i_sb, block, bsize, NULL,
				       actor);
	return res < 0 ? res : 0;
}
