#include <stdio.h>
#include "trulyhandsfree.h"

void main()
{
	thf_t *p_temp;

	p_temp = thfSessionCreate();
	if (p_temp)
		thfSessionDestroy(p_temp);

	printf("hello world!!\n");
	
	return;
}
