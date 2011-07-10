/*-
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <tobias.rehbein@web.de> wrote this file. As long as you retain this notice
 * you can do whatever you want with this stuff. If we meet some day, and you
 * think this stuff is worth it, you can buy me a beer in return.
 *                                                              Tobias Rehbein
 */

/**
 * Required includes:
 *
 * list.h
 */

struct element {
	void		*payload;
	struct element	*next;
};

struct element *create_element(void *_payload);
struct element *destroy_element(struct element *_element);
struct element *prepend_element(struct element *_new, struct element *_list);
