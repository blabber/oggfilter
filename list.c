/*-
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <tobias.rehbein@web.de> wrote this file. As long as you retain this notice
 * you can do whatever you want with this stuff. If we meet some day, and you
 * think this stuff is worth it, you can buy me a beer in return.
 *                                                              Tobias Rehbein
 */

#include <assert.h>
#include <stdlib.h>

#include "list.h"

struct element *
create_element(void *payload)
{
        struct element *new;

        assert(payload != NULL);

        if ((new = malloc(sizeof(*new))) != NULL) {
                new->payload = payload;
                new->next = NULL;
        }
        return (new);
}

struct element *
destroy_element(struct element *element)
{
        struct element *next;

        assert(element !=NULL);

        next = element->next;
        free(element);

        return (next);

}

struct element *
prepend_element(struct element *new, struct element *list)
{
        assert(new != NULL);

        new->next = list;

        return (new);
}
