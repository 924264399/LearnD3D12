#include "Utils.h"
#include <math.h>
#include <algorithm>


float srandom() {
	return (float(rand()) / float(RAND_MAX)) * 2.0f - 1.0f; //rand()返回一个0到RAND_MAX之间的整数  除以RAND_MAX就变成了0到1之间的浮点数 乘以2就变成了0到2之间的浮点数 减去1就变成了-1到1之间的浮点数
}