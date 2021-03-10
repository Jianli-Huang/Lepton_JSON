#ifndef LEFPJSON_H_
#define LEPTJSON_H_

#include <stddef.h>

typedef enum 
{
	LEPT_NULL,		//����������Ϊ NULL
	LEPT_FALSE,		//����������Ϊ FALSE		
	LEPT_TRUE,		//����������Ϊ TRUE
	LEPT_NUMBER,	//����������Ϊ NUMBER		
	LEPT_STRING,	//����������Ϊ STRING
	LEPT_ARRAY,		//����������Ϊ ARRAY
	LEPT_OBJECT		//����������Ϊ OBJECT
} lept_type;

typedef struct lept_value lept_value;
typedef struct lept_member lept_member;

//ÿ���ڵ�ʹ�� lept_value �ṹ���ʾ����ʾΪһ�� JSON ֵ��JSON value��
struct lept_value
{
	union
	{
		//C ���Ե������СӦ��ʹ�� size_t ����
		struct { lept_member* m; size_t size; } o;	// object: members, member count 
		struct { lept_value* e; size_t size; }a;	// array:  elements, element count 
		struct { char* s; size_t len; }s;			// string: null-terminated string, string length 
		double n;									// number 
	}u;
	lept_type type;
};

//�������Object�ĳ�Ա����Ա�����ˡ������͡�ֵ�����Լ��������ĳ���
struct lept_member
{
	char* k; size_t klen;	// member key string, key string length 
	lept_value v;			// member value 
};

enum
{
	LEPT_PARSE_OK = 0,							//�޴���᷵�� LEPT_PARSE_OK
	LEPT_PARSE_EXPECT_VALUE,					//��һ�� JSON ֻ���пհ�
	LEPT_PARSE_INVALID_VALUE,					//�������Ľڵ㲻����JSON�ı�
	LEPT_PARSE_ROOT_NOT_SINGULAR,				//��һ��ֵ֮���ڿհ�֮���������ַ�
	LEPT_PARSE_NUMBER_TOO_BIG,					//���ֹ���
	LEPT_PARSE_MISS_QUOTATION_MARK,				//ȱ������
	LEPT_PARSE_INVALID_STRING_ESCAPE,			//�ַ������ֲ��Ϸ���ת���ַ�
	LEPT_PARSE_INVALID_STRING_CHAR,				//�ַ������ֲ��Ϸ����ַ� ��char < 0x20��
	LEPT_PARSE_INVALID_UNICODE_HEX,				//���\u����4λʮ����λ����
	LEPT_PARSE_INVALID_UNICODE_SURROGATE,		//Unicode�����ֻ�иߴ������Ƿȱ�ʹ�������ǵʹ�����ںϷ���㷶Χ
	LEPT_PARSE_MISS_COMMA_OR_SQUARE_BRACKET,	//ȱ�ٶ���,����������]
	LEPT_PARSE_MISS_KEY,						//������ĳ�Աȱ��keyֵ
	LEPT_PARSE_MISS_COLON,						//�����Աȱ��ð�ţ�
	LEPT_PARSE_MISS_COMMA_OR_CURLY_BRACKET,		//����ȱ�ٶ��ţ����ߴ�����}
};

//��ʼ��
#define lept_init(v) do{(v)->type = LEPT_NULL;} while(0)

//����JSON�����������JSON�ı���һ��C�ַ������ս�β�ַ�����null-terminated string��
int lept_parse(lept_value* v, const char* json);
//���� JSON,���ڵ���ת����JSON�ַ���
char* lept_stringify(const lept_value* v, size_t* length);

//�ͷŽڵ�vռ�õ��ڴ�ռ�
void lept_free(lept_value* v);
#define lept_set_null(v) lept_free(v);

lept_type lept_get_type(const lept_value* v);			//���ʽ���ĺ�������ȡ������

int lept_get_boolean(const lept_value* v);				//��ȡ�ڵ��bool���ͣ�TRUE or FALASE
void lept_set_boolean(lept_value* v, int b);			//���ڵ���������bool���ͣ�TRUE or FALSE

double lept_get_number(const lept_value* v);			//��ȡ�ڵ������
void lept_set_number(lept_value* b, double n);			//���ڵ��������ֺ���Ӧ����

const char* lept_get_string(const lept_value* v);						//��ȡ�ڵ���ַ���
size_t lept_get_string_length(const lept_value* v);						//��ȡ�ڵ���ַ����ĳ���
void lept_set_string(lept_value* v, const char* s, size_t len);			//���ڵ������ַ���

size_t lept_get_array_size(const lept_value* v);						//��ȡ�����Ԫ�ظ���
lept_value* lept_get_array_element(const lept_value* v, size_t index);	//��ȡ�����е�ĳ��Ԫ��

size_t lept_get_object_size(const lept_value* v);						//��ȡ�����Ա����
const char* lept_get_object_key(const lept_value* v, size_t index);		//��ȡ����ĳ����Ա��keyֵ
size_t lept_get_object_key_length(const lept_value* v, size_t index);	//��ȡ����ĳ����Ա��keyֵ����
lept_value* lept_get_object_value(const lept_value* v, size_t index);	//��ȡ����ĳ����Ա��valueֵ

#endif // !LEFPJSON_H_
