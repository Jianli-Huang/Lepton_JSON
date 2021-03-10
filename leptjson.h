#ifndef LEFPJSON_H_
#define LEPTJSON_H_

#include <stddef.h>

typedef enum 
{
	LEPT_NULL,		//解析后类型为 NULL
	LEPT_FALSE,		//解析后类型为 FALSE		
	LEPT_TRUE,		//解析后类型为 TRUE
	LEPT_NUMBER,	//解析后类型为 NUMBER		
	LEPT_STRING,	//解析后类型为 STRING
	LEPT_ARRAY,		//解析后类型为 ARRAY
	LEPT_OBJECT		//解析后类型为 OBJECT
} lept_type;

typedef struct lept_value lept_value;
typedef struct lept_member lept_member;

//每个节点使用 lept_value 结构体表示，表示为一个 JSON 值（JSON value）
struct lept_value
{
	union
	{
		//C 语言的数组大小应该使用 size_t 类型
		struct { lept_member* m; size_t size; } o;	// object: members, member count 
		struct { lept_value* e; size_t size; }a;	// array:  elements, element count 
		struct { char* s; size_t len; }s;			// string: null-terminated string, string length 
		double n;									// number 
	}u;
	lept_type type;
};

//定义对象Object的成员，成员包含了“键”和“值”，以及“键”的长度
struct lept_member
{
	char* k; size_t klen;	// member key string, key string length 
	lept_value v;			// member value 
};

enum
{
	LEPT_PARSE_OK = 0,							//无错误会返回 LEPT_PARSE_OK
	LEPT_PARSE_EXPECT_VALUE,					//若一个 JSON 只含有空白
	LEPT_PARSE_INVALID_VALUE,					//解析出的节点不符合JSON文本
	LEPT_PARSE_ROOT_NOT_SINGULAR,				//若一个值之后，在空白之后还有其他字符
	LEPT_PARSE_NUMBER_TOO_BIG,					//数字过大
	LEPT_PARSE_MISS_QUOTATION_MARK,				//缺少引号
	LEPT_PARSE_INVALID_STRING_ESCAPE,			//字符串出现不合法的转义字符
	LEPT_PARSE_INVALID_STRING_CHAR,				//字符串出现不合法的字符 （char < 0x20）
	LEPT_PARSE_INVALID_UNICODE_HEX,				//如果\u后不是4位十六进位数字
	LEPT_PARSE_INVALID_UNICODE_SURROGATE,		//Unicode，如果只有高代理项而欠缺低代理项，或是低代理项不在合法码点范围
	LEPT_PARSE_MISS_COMMA_OR_SQUARE_BRACKET,	//缺少逗号,或者中括号]
	LEPT_PARSE_MISS_KEY,						//对象里的成员缺少key值
	LEPT_PARSE_MISS_COLON,						//对象成员缺少冒号：
	LEPT_PARSE_MISS_COMMA_OR_CURLY_BRACKET,		//对象缺少逗号，或者大括号}
};

//初始化
#define lept_init(v) do{(v)->type = LEPT_NULL;} while(0)

//解析JSON方法：传入的JSON文本是一个C字符串（空结尾字符串／null-terminated string）
int lept_parse(lept_value* v, const char* json);
//生成 JSON,将节点树转换成JSON字符串
char* lept_stringify(const lept_value* v, size_t* length);

//释放节点v占用的内存空间
void lept_free(lept_value* v);
#define lept_set_null(v) lept_free(v);

lept_type lept_get_type(const lept_value* v);			//访问结果的函数，获取其类型

int lept_get_boolean(const lept_value* v);				//获取节点的bool类型，TRUE or FALASE
void lept_set_boolean(lept_value* v, int b);			//给节点类型设置bool类型，TRUE or FALSE

double lept_get_number(const lept_value* v);			//获取节点的数字
void lept_set_number(lept_value* b, double n);			//给节点设置数字和相应类型

const char* lept_get_string(const lept_value* v);						//获取节点的字符串
size_t lept_get_string_length(const lept_value* v);						//获取节点的字符串的长度
void lept_set_string(lept_value* v, const char* s, size_t len);			//给节点设置字符串

size_t lept_get_array_size(const lept_value* v);						//获取数组的元素个数
lept_value* lept_get_array_element(const lept_value* v, size_t index);	//获取数组中的某个元素

size_t lept_get_object_size(const lept_value* v);						//获取对象成员个数
const char* lept_get_object_key(const lept_value* v, size_t index);		//获取对象某个成员的key值
size_t lept_get_object_key_length(const lept_value* v, size_t index);	//获取对象某个成员的key值长度
lept_value* lept_get_object_value(const lept_value* v, size_t index);	//获取对象某个成员的value值

#endif // !LEFPJSON_H_
