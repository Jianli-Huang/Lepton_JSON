//高版本的VS不让使用较低版本的函数，定义该宏可以忽视警告
#define _CRT_SECURE_NO_WARNINGS			

//用以检测Windows上的code是否有内存泄漏
#ifdef _WINDOWS
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#endif

#include "leptjson.h"
#include <assert.h>		// assert() 
#include <stdlib.h>		// NULL, malloc(), realloc(), free(), strtod() 
#include <errno.h>		// HUGE_VAL 
#include <math.h>		// HUGE_VAL 
#include <string.h>		// memcpy() 
#include <stdio.h>		// sprintf() 

//解析JSON的字符串时，初始给栈分配的内存空间
#ifndef LEPT_PARSE_STACK_INIT_SIZE
#define LEPT_PARSE_STACK_INIT_SIZE 256
#endif 

//生成JSON时，初始给栈分配的内存空间
#ifndef LEPT_PARSE_STRINGIFY_INT_SIZE
#define LEPT_PARSE_STRINGIFY_INT_SIZE 256
#endif

//JSON字符串当前字符和字符ch相同时，访问当前字符的下一个字符
#define EXPECT(c, ch)       do { assert(*c->json == (ch)); c->json++; } while(0)
//判断ch是否位于0~9数字范围内
#define ISDIGIT(ch)         ((ch) >= '0' && (ch) <= '9')
//判断ch是否位于1~9数字范围内
#define ISDIGIT1TO9(ch)     ((ch) >= '1' && (ch) <= '9')
//将字符ch入栈，lept_context_push用以分配空间，并返回指针，最后将字符ch存储到指针指向的内存空间
#define PUTC(c, ch)         do { *(char*)lept_context_push(c, sizeof(char)) = (ch); } while(0)
//将字符串入栈，使用memcpy函数将字符串s进行内存拷贝到栈中
#define PUTS(c, s, len)		memcpy(lept_context_push(c, len), s, len);

//为了减少解析函数之间传递多个参数，把这些数据都放进一个 lept_context 结构体内
typedef struct
{
	const char* json;		//存储json字符串的当前位置
	char* stack;			//栈内存空间
	size_t size, top;		//size:栈的空间大小  top：栈顶	初始栈空间大小为0，栈顶为0
}lept_context;

//栈内分配size大小的空间，准备存储数据，并修改栈顶
//若栈内存不够还需要扩充
//栈是以字节储存的。每次可要求压入任意大小的数据，它会返回数据起始的指针
static void* lept_context_push(lept_context* c, size_t size)
{
	void* ret;	//返回分配好空间的地址
	assert(size > 0);
	if (c->top + size >= c->size)	//目前栈空间不足以存储新元素
	{
		if (c->size == 0)		//第一次存储数据，分配栈空间
			c->size = LEPT_PARSE_STACK_INIT_SIZE;	
		while (c->top + size >= c->size)
			c->size += c->size >> 1;	//扩充栈内存  c->size * 1.5 
		c->stack = (char*)realloc(c->stack, c->size);	//重新分配空间，并将旧空间的元素拷贝到新空间
	}
	ret = c->stack + c->top;	//要入栈元素指向的内存空间
	c->top += size;		//更新栈顶
	return ret;
}

//元素出栈
static void* lept_context_pop(lept_context* c, size_t size)
{
	assert(c->top >= size);
	return c->stack + (c->top -= size);		//要出栈元素在栈中的首地址
}

//解析空白字符，直至没有遇到空白字符
static void lept_parse_whitespace(lept_context* c)
{
	const char* p = c->json;
	//空格符（space U+0020）、制表符（tab U+0009）、换行符（LF U+000A）、回车符（CR U+000D）所组成。
	while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')
	{
		p++;
	}
	c->json = p;
}

//解析 NULL/TRUE/FALSE 三种字面值
static int lept_parse_literal(lept_context* c, lept_value* v, const char* literal, lept_type type)
{
	size_t i;
	EXPECT(c, literal[0]);
	for (i = 0; literal[i + 1]; i++)
	{
		if (c->json[i] != literal[i + 1])
		{
			return LEPT_PARSE_INVALID_VALUE;	//字面值不符合 NULL/TRUE/FALSE 这三种情况
		}
	}
	c->json += i;
	v->type = type;
	return LEPT_PARSE_OK;
}

//解析数字：数字语法
//number = ["-"] int[frac][exp]
//int = "0" / digit1 - 9 * digit
//frac = "." 1 * digit
//exp = ("e" / "E")["-" / "+"] 1 * digit
static int lept_parse_number(lept_context* c, lept_value* v)
{
	const char* p = c->json;
	if (*p == '-') p++;
	if (*p == '0') p++;
	else
	{
		//整数部分，若至少有两个数字，第一个数字不能为数字0，与八进制数字格式相同
		if (!ISDIGIT1TO9(*p)) return LEPT_PARSE_INVALID_VALUE;	
		for (p++; ISDIGIT(*p); p++);
	}
	if (*p == '.')
	{
		p++;
		if (!ISDIGIT(*p)) return LEPT_PARSE_INVALID_VALUE;
		for (p++; ISDIGIT(*p); p++);
	}
	if (*p == 'e' || *p == 'E')
	{
		p++;
		if (*p == '+' || *p == '-') p++;
		if (!ISDIGIT(*p)) return LEPT_PARSE_INVALID_VALUE;
		for (p++; ISDIGIT(*p); p++);
	}
	//errno 是记录系统的最后一次错误代码。代码是一个int型的值，在errno.h中定义。
	errno = 0;
	//strtod将字符串转换成double类型
	v->u.n = strtod(c->json, NULL);
	//超出数学函数定义的范围
	if (errno == ERANGE && (v->u.n == HUGE_VAL || v->u.n == -HUGE_VAL))
	{
		return LEPT_PARSE_NUMBER_TOO_BIG;
	}
	v->type = LEPT_NUMBER;
	c->json = p;
	return LEPT_PARSE_OK;
}

//解析4位十六进制数字，将0~F转换成0~15
static const char* lept_parse_hex4(const char* p, unsigned* u)
{
	int i;
	*u = 0;
	for (i = 0; i < 4; i++)
	{
		char ch = *p++;
		*u <<= 4;
		if (ch >= '0' && ch <= '9') *u |= ch - '0';
		else if (ch >= 'A' && ch <= 'F') *u |= ch - ('A' - 10);
		else if (ch >= 'a' && ch <= 'f') *u |= ch - ('a' - 10);
		else return NULL;
	}
	return p;
}

static void lept_encode_utf8(lept_context* c, unsigned u)
{
	if (u <= 0x7F)	//码点范围U+0000 ~ U+007F
	{
		PUTC(c, u & 0xFF);
	}
	else if (u <= 0x7FF)	//U + 0080 ~ U + 07FF
	{
		PUTC(c, 0xC0 | ((u >> 6) & 0xFF));
		PUTC(c, 0x80 | (u & 0x3F));
	}
	else if (u <= 0xFFFF)	//U + 0800 ~ U + FFFF
	{
		PUTC(c, 0xE0 | ((u >> 12) & 0xFF));
		PUTC(c, 0x80 | ((u >> 6)  & 0x3F));
		PUTC(c, 0x80 | (u & 0x3F));
	}
	else {	//U+10000 ~ U+10FFFF
		assert(u <= 0x10FFFF);
		PUTC(c, 0xF0 | ((u >> 18) & 0xFF));
		PUTC(c, 0x80 | ((u >> 12) & 0x3F));
		PUTC(c, 0x80 | ((u >> 6) & 0x3F));
		PUTC(c, 0x80 | (u & 0x3F));
	}
}

//解析失败，要把先前压入栈的元素弹出，所以只需修改栈顶指针
#define STRING_ERROR(ret) do{c->top = head; return ret;} while(0)

//解析字符串并压栈
static int lept_parse_string_raw(lept_context* c, char** str, size_t* len) 
{
	size_t head = c->top;	//备份栈顶
	unsigned u, u2;
	const char* p;
	EXPECT(c, '\"');	//指明要解析的是字符串
	p = c->json;
	for (;;) {
		char ch = *p++;
		switch (ch) {
		case '\"':		//要么是空字符，要么本次字符串解析完成
			*len = c->top - head;	//获取字符串的长度
			*str = lept_context_pop(c, *len);	//字符串压栈后，返回其入栈的首元素的地址
			c->json = p;
			return LEPT_PARSE_OK;
		case '\\':		//转义字符
			switch (*p++) {
			case '\"': PUTC(c, '\"'); break;
			case '\\': PUTC(c, '\\'); break;
			case '/':  PUTC(c, '/'); break;
			case 'b':  PUTC(c, '\b'); break;
			case 'f':  PUTC(c, '\f'); break;
			case 'n':  PUTC(c, '\n'); break;
			case 'r':  PUTC(c, '\r'); break;
			case 't':  PUTC(c, '\t'); break;
			case 'u':		//\uXXXX解析
				if (!(p = lept_parse_hex4(p, &u)))		//解析
					STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX);
				if (u >= 0xD800 && u <= 0xDBFF)		//代理对表示 \uXXXX\uYYYY
				{  
					if (*p++ != '\\')
						STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE);
					if (*p++ != 'u')
						STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE);
					if (!(p = lept_parse_hex4(p, &u2)))
						STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_HEX);
					if (u2 < 0xDC00 || u2 > 0xDFFF)
						STRING_ERROR(LEPT_PARSE_INVALID_UNICODE_SURROGATE);
					//codepoint = 0x10000 + (H − 0xD800) × 0x400 + (L − 0xDC00)
					u = (((u - 0xD800) << 10) | (u2 - 0xDC00)) + 0x10000;
				}
				lept_encode_utf8(c, u);
				break;
			default:
				STRING_ERROR(LEPT_PARSE_INVALID_STRING_ESCAPE);
			}
			break;
		case '\0':	//空字符
			STRING_ERROR(LEPT_PARSE_MISS_QUOTATION_MARK);	//字符串异常结束，缺少下一个双引号"
		default:
			if ((unsigned char)ch < 0x20)
				STRING_ERROR(LEPT_PARSE_INVALID_STRING_CHAR);
			PUTC(c, ch);
		}
	}
}

//解析字符串并把栈中的字符串拷贝到节点中
static int lept_parse_string(lept_context* c, lept_value* v)
{
	int ret;
	char* s;
	size_t len;
	if ((ret = lept_parse_string_raw(c, &s, &len)) == LEPT_PARSE_OK)	//解析字符串到栈中
		lept_set_string(v, s, len);		//给节点设置解析好的字符串	
	return ret;
}

static int lept_parse_value(lept_context* c, lept_value* v);

//解析数组
static int lept_parse_array(lept_context* c, lept_value* v)
{
	size_t i, size = 0;
	int ret;
	EXPECT(c, '[');		//指明要解析的是数组
	lept_parse_whitespace(c);	//跳过空白字符
	if (*c->json == ']')	//解析的是一个空数组
	{
		c->json++;
		v->type = LEPT_ARRAY;
		v->u.a.size = 0;
		v->u.a.e = NULL;
		return LEPT_PARSE_OK;
	}
	for (;;)
	{
		lept_value e;
		lept_init(&e);
		if ((ret = lept_parse_value(c, &e)) != LEPT_PARSE_OK)	//递归调用，数组中的元素可能是 字面量、数字、字符串、数组、对象
			break;
		//将解析后的 lept_value e 拷贝到栈中
		memcpy(lept_context_push(c, sizeof(lept_value)), &e, sizeof(lept_value));
		size++;		//数组元素个数加1
		lept_parse_whitespace(c);
		if (*c->json == ',')	//解析下一个数组元素
		{
			c->json++;
			lept_parse_whitespace(c);
		}
		else if (*c->json == ']')	//解析完成
		{
			c->json++;
			v->type = LEPT_ARRAY;
			v->u.a.size = size;
			size *= sizeof(lept_value);
			//栈只是临时的，需要把解析后得到的元素拷贝到定义好的节点的内存空间，再释放栈内存
			memcpy(v->u.a.e = (lept_value*)malloc(size), lept_context_pop(c, size), size);
			return LEPT_PARSE_OK;
		}
		else
		{
			ret = LEPT_PARSE_MISS_COMMA_OR_SQUARE_BRACKET;		//缺少逗号,或者中括号]
			break;
		}
	}
	//释放栈内存
	for (i = 0; i < size; i++)
		lept_free((lept_value*)lept_context_pop(c, sizeof(lept_value)));
	return ret;
}

//解析对象
static int lept_parse_object(lept_context* c, lept_value* v)
{
	size_t i, size;
	lept_member m;	//临时变量，存储解析好的对象成员
	int ret;
	EXPECT(c, '{');		//指明要解析的是对象
	lept_parse_whitespace(c);
	if (*c->json == '}')	//对象成员为空
	{
		c->json++;
		v->type = LEPT_OBJECT;
		v->u.o.m = 0;
		v->u.o.size = 0;
		return LEPT_PARSE_OK;
	}
	m.k = NULL;
	size = 0;
	for (;;)
	{
		char* str;
		lept_init(&m.v);
		if (*c->json != '"')	//解析key值，key值是一个字符串
		{
			ret = LEPT_PARSE_MISS_KEY;		//成员缺少key值
			break;
		}
		if ((ret = lept_parse_string_raw(c, &str, &m.klen)) != LEPT_PARSE_OK)	//字符串解析失败
			break;
		memcpy(m.k = (char*)malloc(m.klen + 1), str, m.klen);
		m.k[m.klen] = '\0';		//给字符串补上空字符'\0' 
		lept_parse_whitespace(c);
		if (*c->json != ':')	
		{
			ret = LEPT_PARSE_MISS_COLON;	//缺少冒号
			break;
		}
		c->json++;
		lept_parse_whitespace(c);
		if ((ret = lept_parse_value(c, &m.v)) != LEPT_PARSE_OK)		//解析成员里的value值
			break;
		memcpy(lept_context_push(c, sizeof(lept_member)), &m, sizeof(lept_member));		//将解析好的成员拷贝到栈内存中
		size++;
		m.k = NULL;		//此时成员的所有权已转移到栈中
		lept_parse_whitespace(c);
		if (*c->json == ',')	//解析下一个成员
		{
			c->json++;
			lept_parse_whitespace(c);
		}
		else if (*c->json == '}')	//解析结束
		{
			size_t s = sizeof(lept_member) * size;
			c->json++;
			v->type = LEPT_OBJECT;
			v->u.o.size = size;
			memcpy(v->u.o.m = (lept_member*)malloc(s), lept_context_pop(c, s), s);		//将栈中所有解析好的成员拷贝到节点中
			return LEPT_PARSE_OK;
		}
		else
		{
			ret = LEPT_PARSE_MISS_COMMA_OR_CURLY_BRACKET;		//对象缺少逗号，或者大括号}
			break;
		}
	}
	
	free(m.k);	//释放临时变量所占用内存
	for (i = 0; i < size; i++)
	{
		//释放栈中的内存空间
		lept_member* m = (lept_member*)lept_context_pop(c, sizeof(lept_member));
		free(m->k);
		lept_free(&m->v);	//递归释放
	}
	v->type = LEPT_NULL;
	return ret;
}

static int lept_parse_value(lept_context* c, lept_value* v)
{
	switch (*c->json)
	{
	case 't':	return lept_parse_literal(c, v, "true", LEPT_TRUE);
	case 'f':	return lept_parse_literal(c, v, "false", LEPT_FALSE);
	case 'n':	return lept_parse_literal(c, v, "null", LEPT_NULL);
	default:	return lept_parse_number(c, v);
	case '"':	return lept_parse_string(c, v);
	case '[':	return lept_parse_array(c, v);
	case '{':	return lept_parse_object(c, v);
	case '\0':	return LEPT_PARSE_EXPECT_VALUE;		//JSON 只含有空白
	}
}

//JSON-text = ws value ws
//ws = *(% x20 / % x09 / % x0A / % x0D)
//value = null / false / true
//null = "null"
//false = "false"
//true = "true"
int lept_parse(lept_value* v, const char* json)
{
	lept_context c;		//备份json，并使用栈保存json解析后的内容
	int ret;
	assert(v != NULL);
	c.json = json;
	c.stack = NULL;
	c.size = c.top = 0;
	lept_init(v);
	lept_parse_whitespace(&c);
	if ((ret = lept_parse_value(&c, v)) == LEPT_PARSE_OK)
	{
		lept_parse_whitespace(&c);
		if (*c.json != '\0')	//解析一个值之后，在空白之后还有其他字符
		{
			v->type = LEPT_NULL;
			ret = LEPT_PARSE_ROOT_NOT_SINGULAR;
		}
	}
	//不管解析的是不是字符串，最后c.top = 0，因为数据都被弹出了，而且下次可复用栈
	assert(c.top == 0);
	free(c.stack);
	return ret;
}

//字符串化，生成字符串并压入栈
//最后没有往栈中压入空字符'\0'的原因：解析的字符串只是JSON的一个节点，若加入了空字符，则表示JSON到此结束
//'\0'表示JSON字符串的结束
static void lept_stringify_string(lept_context* c, const char* s, size_t len)
{
	static const char hex_digits[] = { '0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F' };
	size_t i, size;
	char* head, * p;
	assert(s != NULL);
	//往栈中压入字符时，一开始和最后都要压入双引号字符"
	p = head = lept_context_push(c, size = len * 6 + 2);	// "\u00xx..."
	*p++ = '"';
	for (i = 0; i < len; i++)
	{
		unsigned char ch = (unsigned char)s[i];
		switch (ch)
		{
		//转义字符
		case '\"': *p++ = '\\'; *p++ = '\"'; break;
		case '\\': *p++ = '\\'; *p++ = '\\'; break;
		case '\b': *p++ = '\\'; *p++ = 'b'; break;
		case '\f': *p++ = '\\'; *p++ = 'f'; break;
		case '\n': *p++ = '\\'; *p++ = 'n'; break;
		case '\r': *p++ = '\\'; *p++ = 'r'; break;
		case '\t': *p++ = '\\'; *p++ = 't'; break;
		default:
			if (ch < 0x20)
			{
				//以\u00xx表示
				*p++ = '\\'; *p++ = 'u'; *p++ = '0'; *p++ = '0';
				*p++ = hex_digits[ch >> 4];
				*p++ = hex_digits[ch & 15];
			}
			else
			{
				*p++ = s[i];
			}
		}
	}
	*p++ = '"';
	c->top -= size - (p - head);	//调整栈顶指针
}

//生成JSON
static void lept_stringify_value(lept_context* c, const lept_value* v)
{
	size_t i;
	switch (v->type)
	{
	case LEPT_NULL:		PUTS(c, "null", 4); break;
	case LEPT_FALSE:	PUTS(c, "false", 5); break;
	case LEPT_TRUE:		PUTS(c, "true", 4); break;
	//把浮点数转换成文本，"%.17g" 是足够把双精度浮点转换成可还原的文本。
	case LEPT_NUMBER:	c->top -= 32 - sprintf(lept_context_push(c, 32), "%.17g", v->u.n); break;	
	case LEPT_STRING:	lept_stringify_string(c, v->u.s.s, v->u.s.len); break;
	case LEPT_ARRAY:	
		PUTC(c, '[');
		for (i = 0; i < v->u.a.size; i++)
		{
			if (i > 0)
				PUTC(c, ',');
			lept_stringify_value(c, &v->u.a.e[i]);	//生成数组中的元素
		}
		PUTC(c, ']');
		break;
	case LEPT_OBJECT:
		PUTC(c, '{');
		for (i = 0; i < v->u.o.size; i++)
		{
			if (i > 0)
				PUTC(c, ',');
			lept_stringify_string(c, v->u.o.m[i].k, v->u.o.m[i].klen);	//生成对象中成员的key
			PUTC(c, ':');
			lept_stringify_value(c, &v->u.o.m[i].v);		//生成对象中成员的value
		}
		PUTC(c, '}');
		break;
	default: assert(0 && "invalid type");
	}
}

//将节点树字符串化生成JSON文本
char* lept_stringify(const lept_value* v, size_t* length)
{
	lept_context c;
	assert(v != NULL);
	c.stack = (char*)malloc(c.size = LEPT_PARSE_STACK_INIT_SIZE);	//分配栈内存，用以存储生成的JSON
	c.top = 0;
	lept_stringify_value(&c, v);
	if (length)
		*length = c.top;	//记录生成的JSON长度
	PUTC(&c, '\0');
	return c.stack;
}

//释放节点所占的内存
void lept_free(lept_value* v)
{
	size_t i;
	assert(v != NULL);
	switch (v->type)
	{
	case LEPT_STRING:	//释放字符串
		free(v->u.s.s);
		break;
	case LEPT_ARRAY:
		for (i = 0; i < v->u.a.size; i++)
			lept_free(&v->u.a.e[i]);	//递归释放数组各个元素所指向的内存空间
		free(v->u.a.e);		//最后释放数组指针
		break;
	case LEPT_OBJECT:
		for (i = 0; i < v->u.o.size; i++)
		{
			free(v->u.o.m[i].k);	//释放对象中成员的key值
			lept_free(&v->u.o.m[i].v);		//递归释放对象中成员的value值
		}
		free(v->u.o.m);		//最后释放对象成员指针
		break;
	default:
		break;
	}
	v->type = LEPT_NULL;
}

//获取节点类型
lept_type lept_get_type(const lept_value* v)
{
	assert(v != NULL);
	return v->type;
}

//获取节点的bool类型，TRUE or FALASE
int lept_get_boolean(const lept_value* v)
{
	assert(v != NULL && (v->type == LEPT_TRUE || v->type == LEPT_FALSE));
	return v->type == LEPT_TRUE;
}

//给节点类型设置bool类型，TRUE or FALSE
void lept_set_boolean(lept_value* v, int b)
{
	lept_free(v);
	v->type = b ? LEPT_TRUE : LEPT_FALSE;
}

//获取节点的数字
double lept_get_number(const lept_value* v)
{
	assert(v != NULL && v->type == LEPT_NUMBER);
	return v->u.n;
}

//给节点设置数字和相应类型
void lept_set_number(lept_value* v, double n)
{
	lept_free(v);
	v->u.n = n;
	v->type = LEPT_NUMBER;
}

//获取节点的字符串
const char* lept_get_string(const lept_value* v)
{
	assert(v != NULL && v->type == LEPT_STRING);
	return v->u.s.s;
}

//获取节点的字符串的长度
size_t lept_get_string_length(const lept_value* v)
{
	assert(v != NULL && v->type == LEPT_STRING);
	return v->u.s.len;
}

//给节点设置字符串
void lept_set_string(lept_value* v, const char* s, size_t len)
{
	assert(v != NULL && (s != NULL || len == 0));
	lept_free(v);
	v->u.s.s = (char*)malloc(len + 1);
	memcpy(v->u.s.s, s, len);
	v->u.s.s[len] = '\0';
	v->u.s.len = len;
	v->type = LEPT_STRING;
}

//获取数组的元素个数
size_t lept_get_array_size(const lept_value* v)
{
	assert(v != NULL && v->type == LEPT_ARRAY);
	return v->u.a.size;
}

//获取数组中的某个元素
lept_value* lept_get_array_element(const lept_value* v, size_t index)
{
	assert(v != NULL && v->type == LEPT_ARRAY);
	assert(index < v->u.a.size);
	return &v->u.a.e[index];
}

//获取对象成员个数
size_t lept_get_object_size(const lept_value* v)
{
	assert(v != NULL && v->type == LEPT_OBJECT);
	return v->u.o.size;
}

//获取对象某个成员的key值
const char* lept_get_object_key(const lept_value* v, size_t index)
{
	assert(v != NULL && v->type == LEPT_OBJECT);
	assert(index < v->u.o.size);
	return v->u.o.m[index].k;
}

//获取对象某个成员的key值长度
size_t lept_get_object_key_length(const lept_value* v, size_t index)
{
	assert(v != NULL && v->type == LEPT_OBJECT);
	assert(index < v->u.o.size);
	return v->u.o.m[index].klen;
}

//获取对象某个成员的value值
lept_value* lept_get_object_value(const lept_value* v, size_t index)
{
	assert(v != NULL && v->type == LEPT_OBJECT);
	assert(index < v->u.o.size);
	return &v->u.o.m[index].v;
}
