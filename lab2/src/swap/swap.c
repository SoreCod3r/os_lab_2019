#include "swap.h"

void Swap(char *left, char *right)
{
	char var = *left;
	*left = *right;
	*right = var;
}
