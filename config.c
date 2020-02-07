#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "include/debug.h"

struct kv {
    char segment[20];
    char key[20];
    char value[50];
};

struct kv *keymap;
int keymap_count;

FILE *fp;

//（空白符指空格、水平制表、垂直制表、换页、回车和换行符）
#define isspace(c) ((c) == ' ' || (c) == '\t' || \
					(c) == '\r' || (c) == '\n')

char* skip_space(const char* str)
{
    while (isspace(*str))
		++str;
	return (char *)str;
}

char *strim(char *s)
{
	size_t size;
	char *end;

	size = strlen(s);
	if (!size)
		return s;

	end = s + size - 1;
	while (end >= s && isspace(*end))
		end--;
	*(++end) = '\0';

	return skip_space(s);
}

int load_profile(const char *filename, int size)
{
    char buf[100] = {0};
    char segment[20] = "";
    char *s = NULL;
    struct kv *element;
    fp = fopen(filename, "r");
    if (fp == NULL) {
        pr_err("profile is not exsit! filename:%s\n", filename);
        return -1;
    }
    keymap = (struct kv *)malloc(size * sizeof(struct kv));
    if (!keymap) {
        err_exit("malloc memory failed\n");
    }
    element = keymap;
    while (fgets(buf, 100, fp)) {
        s = strim(buf);
        pr_info("strimed string:%s\n", s);
        if (strlen(s) == 0 || *s == '#') {
            continue;
        } else if (*s == '[') {
            sscanf(s, "[%[^]]", segment);
            pr_err("match segment: %s\n", segment);
        } else {
            sscanf(s, "%[^=]=%s", element->key, element->value);
            strncpy(element->segment, segment, 20);
            pr_err("match element:[%s] %s - %s\n", element->segment, element->key, element->value);
            element++;
			keymap_count++;
        }
    }
    return 0;
}

int get_key_value(const char *segment, const char *key, char *value)
{
	int i;
	struct kv *kv;

	for (i = 0; i < keymap_count; i++)
	{
		kv = &keymap[i];
		if (!strcmp(segment, kv->segment) && !strcmp(key, kv->key))
			strcpy(value, kv->value);
			return 0;
	}
	return -1;
}

int get_keymap_count(void)
{
	return keymap_count;
}

/*Test case*/
/*
int main(int argc, char *argv[])
{
	char value[50];

    if (argc == 1)
        err_exit("Please give a filename for parse!\n");
    load_profile(argv[1], 20);
	get_key_value("123", "xie", value);
	printf("keymap count:%d\n", get_keymap_count());
	printf("key value: xie--%s\n", value);
    return 0;
}
*/
