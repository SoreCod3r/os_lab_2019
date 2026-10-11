#include "revert_string.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"

void RevertString(char *str)
{
	char *reverted_str = malloc(sizeof(char) * (strlen(str) + 1));
	int j = 0;
	for (int i = strlen(str) - 1; i >= 0; i--)
	{
		reverted_str[j] = str[i];
		j++;
	}
	reverted_str[j] = '\0';
	strcpy(str, reverted_str);
	free(reverted_str);
}

