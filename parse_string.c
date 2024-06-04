#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "cJSON.h"
#include <stdbool.h>

//gcc -I ./cjson -o parse_string parse_string.c cJSON.c

int div_up(int x, int y)
{
    return (x - 1) / y + 1;
}

void shift_left_with_carry(uint16_t *array, size_t size, int shift) {
    uint16_t carry = 0; 
    for (size_t i = 0; i < size; i++) {
        uint16_t next_carry = (array[i] >> (16 - shift));
        array[i] = (array[i] << shift) | carry;
        carry = next_carry; 
    }
}

void shift_right_with_carry(uint16_t *array, size_t size, int shift) {
    uint16_t carry = 0; 
    for (size_t i = size; i > 0; i--) {
        uint16_t next_carry = (array[i - 1] & ((1 << shift) - 1)) << (16 - shift); 
        array[i - 1] = (array[i - 1] >> shift) | carry; 
        carry = next_carry; 
    }
}


int parse_param(const cJSON* param, const cJSON* json, void* sourse, char* destination, uint8_t* used_ouput){

	char* param_name = param->child->string;
	if (!strcmp(param_name, "realVal")){
		uint16_t val = *(int*)sourse;
		uint16_t searchVal = cJSON_GetObjectItem(param, "val")->valueint;
		if (val == searchVal){
			if (cJSON_IsTrue(param->child)){
				strcpy(destination, "true");
			} else if (cJSON_IsFalse(param->child)){
				strcpy(destination, "false");
			} else {
				sprintf(destination, "%d\0", cJSON_GetObjectItem(param, "realVal")->valueint);
			}
			*used_ouput+=1;
			return 1;
		}
	} else if (!strcmp(param_name, "min")){
		uint16_t val = *(int*)sourse;
		val += cJSON_GetObjectItem(param, "step")->valueint;
		if (val< cJSON_GetObjectItem(param, "min")->valueint){
			val = cJSON_GetObjectItem(param, "min")->valueint;
		} 
		if (val > cJSON_GetObjectItem(param, "max")->valueint){
			val = cJSON_GetObjectItem(param, "max")->valueint;
		} 
		sprintf(destination, "%d", val);
		*used_ouput+=1;
		return 0;
	} else if (!strcmp(param_name, "paramLen")) {
		uint8_t word_shift = cJSON_GetObjectItem(param, "word")->valueint - cJSON_GetObjectItem(json, "word")->valueint;
		uint8_t bit_shift = cJSON_GetObjectItem(param, "bit")->valueint - cJSON_GetObjectItem(json, "bit")->valueint;
		uint8_t char_len = cJSON_GetObjectItem(param, "len")->valueint/8;
		uint8_t start_position = word_shift*2 + bit_shift/8;
		snprintf(destination + *used_ouput, char_len+2, "%.*s \0",char_len, (char*)sourse+start_position);
		*used_ouput += char_len+1;
		return 0;
	}
	return 0;
}

void parse_line(const cJSON* json, const char* line){
	
	cJSON* jname =  cJSON_GetObjectItem(json, "name");

	char name[30];
	if (cJSON_IsString(jname)) {
		strcpy(name, jname->valuestring);
	} else if (cJSON_IsNumber(jname)) {
		sprintf(name, "%d", jname->valueint);
	}

	uint8_t word = cJSON_GetObjectItem(json, "word")->valueint;
	uint8_t bit = cJSON_GetObjectItem(json, "bit")->valueint;
	uint8_t len = cJSON_GetObjectItem(json, "len")->valueint;

	uint8_t byte_len = (len - 1) / 8 + 1;
	if (strlen(line)-(word-1)*2 - byte_len  < 0){
		printf("Error: Out of range bites.\n"); 
		return;
	}

	uint8_t val_words_size = (bit+len-1)/16+1;
	uint16_t* val_words = (uint16_t*) malloc(sizeof(uint16_t)*val_words_size);
	for (uint8_t byte=0; byte < (byte_len+byte_len%2)/2; ++byte){
		val_words[byte] = 0;
	}

	for (uint8_t curr_word= word+(len+bit - 1) / 16; curr_word >= word; --curr_word) {
		
		char hex_string[4];
		strncpy(hex_string, line + (word-1)*4, 4);
		uint16_t parsing_word;
    	sscanf(hex_string, "%hx", &parsing_word);
		uint8_t extracted_bits_len;

		if (curr_word == word){
			extracted_bits_len = len; 
			shift_left_with_carry(val_words, val_words_size, extracted_bits_len);

			uint16_t mask = ((1 << extracted_bits_len) - 1) << bit;
			uint16_t extracted_bits = ( parsing_word & mask) >> bit;

			val_words[val_words_size-1] |= extracted_bits;
		} else {
			extracted_bits_len = (len + bit) % 16;
			shift_left_with_carry(val_words, val_words_size, extracted_bits_len);

			uint16_t mask = (1 << extracted_bits_len) - 1;
			uint16_t extracted_bits = parsing_word & mask;

			val_words[val_words_size-1] |= extracted_bits;
			len -= extracted_bits_len;
		}
	} 

	uint8_t used_output = 0;
	char* output_string = malloc(sizeof(char)*16);
	void* val;
	//get value
	char* val_type = cJSON_GetObjectItem(json, "dataType")->valuestring;
	if (!strcmp(val_type, "int")  || !strcmp(val_type, "bool") ){
		val = malloc(sizeof(int));
		*(uint16_t *)val = 0;
		uint16_t ans = (int)val_words[val_words_size-1];
		*(uint16_t *)val = (int)val_words[val_words_size-1];
	} else if (!strcmp(val_type, "string")){
		len = (cJSON_GetObjectItem(json, "len")->valueint -1)/8/sizeof(char) +1;
		val = (char*)malloc(sizeof(char)*(len+1)); 
		for (size_t i=0; i<len; ++i){
			 char char_to_copy = (char)(val_words[0] & 0x00FF);
    		((char *)val)[i] = char_to_copy;
			shift_right_with_carry(val_words, val_words_size, sizeof(char)*8);
		}
		((char *)val)[len] = '\0';
	}
	

	cJSON* params = cJSON_GetObjectItem(json, "params");
	if (cJSON_IsArray(params)){
		int size = cJSON_GetArraySize(params);
		for (int i = 0; i < size; i++)
		{
			int solved = parse_param(cJSON_GetArrayItem(params, i), json, val, output_string, &used_output);
			if (solved) break;
		}
	} else if (cJSON_IsObject(params)){
		parse_param(params, json, val, output_string, &used_output);
	}else if (cJSON_IsString(params)) {
		sprintf(output_string, "%s", (char*)val);
		used_output+=1;
	} 
	if (used_output == 0)
		sprintf(output_string, "%d", *(uint16_t *)val);
	int params_type = cJSON_GetObjectItem(json, "dataType")->type;
	free(val);
	printf("%s: %s\n", name, output_string);
	free(val_words);
}

int main(){
	char* parse_string = "043200b4000000000000010000b4003b00000000100301001f0000000000000000b400b501001f003c";
	
	// open json file
	FILE* fp = fopen("mc_test.json", "r"); 
    if (fp == NULL) { 
        printf("Error: Unable to open the file.\n"); 
        exit(1);
		//return 1; 
    } 
  
    // read the file contents into a string 
    char buffer[8192];
	const char *end_ptr;
	int len = fread(buffer, 1, sizeof(buffer), fp);

	//find start read pos in json 
	const char * position =  strchr(buffer, '[');
	int is_parsing = 1;
	cJSON* json;
	int shift = 0;
	
	while (is_parsing){
		//read new part of file
		fseek(fp, position-buffer+shift, SEEK_SET);
		shift += position-buffer;
		len = fread(buffer, sizeof(char), sizeof(buffer), fp); 
		position = buffer;

		while (is_parsing)
		{
			//json struct parse
			json = cJSON_ParseWithLengthOpts(position+1, len-(position-buffer)-1, &end_ptr, 0);
			
			//check json
			if (json == NULL) { 
				const char *error_ptr = cJSON_GetErrorPtr(); 
				if (error_ptr != NULL) { 
					//printf("Error: %s\n", error_ptr); 
				} 
				break;
			} 

			parse_line(json, parse_string);

			if (*end_ptr != ',')
				is_parsing = 0;

			position = end_ptr;
		}
	}
	
	fclose(fp);
	cJSON_Delete(json);
	
    return 0;
}