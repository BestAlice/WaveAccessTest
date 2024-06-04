#include <stdio.h>
#include <stdint.h>

int main()
{
    uint32_t nums[4];
	uint32_t max_sum = 0;
	for (size_t i=0; i<4; ++i){
		scanf("%u", &nums[i]);
		if (nums[i] > max_sum){
			max_sum = nums[i];
		}
	}
	
	for (size_t i=0; i<4; ++i){
		if (max_sum == nums[i]) continue;
		printf( "%u ", max_sum - nums[i]);
	}

    return 0;
}