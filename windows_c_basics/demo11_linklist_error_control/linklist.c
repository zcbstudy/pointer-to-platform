/*
 *            _ooOoo_
 *           o8888888o
 *           88" . "88
 *           (| -_- |)
 *            O\ = /O
 *        ____/`---'\____
 *      .   ' \\| |// `.
 *       / \\||| : |||// \
 *     / _||||| -:- |||||- \
 *       | | \\\ - /// | |
 *     | \_| ''\---/'' | |
 *      \ .-\__ `-` ___/-. /
 *   ___`. .' /--.--\ `. . __
 * ."" '< `.___\_<|>_/___.' >'"".
 * | | : `- \`.;`\ _ /`;.`/ - ` : | |
 * \ \ `-. \_ __\ /__ _/ .-` / /
 *======`-.____`-.___\_____/___.-`____.-'======
 *             `=---='
 *
 *        赛博佛祖保佑
 *        指针不野，内存不漏
 *        head 不丢，tail 不乱
 *        malloc 必成，free 不炸
 *        编译一次过，运行无段错
 */
#include <stdio.h>
#include <stdlib.h>
#include "linklist.h"

//创建链表
int create(struct Node** head, struct Node** tail, int input)
{
	if (head == NULL || tail == NULL)
	{
		return LIST_ERR_NULL;
	}

	if (*head != NULL || *tail != NULL)
	{
		return LIST_ERR_INVALID;
	}

	struct Node* new_node = malloc(sizeof(struct Node));
	if (new_node == NULL)
	{
		return LIST_ERR_MALLOC;
	}

	new_node->data = input;
	new_node->next = NULL;

	*head = new_node;
	*tail = new_node;

	return LIST_OK;
}

//查找函数
struct Node* find(struct Node* head, int find_data)
{
	struct Node* p = head;
	while (p != NULL)
	{
		if (p->data == find_data)
			break;
		else
			p = p->next;
	}
	return p;
}

//头插法
int insert_head(struct Node** head, int input_head)
{
	struct Node* p = NULL;
	if (head == NULL)
	{
		return LIST_ERR_NULL;
	}
	if (*head == NULL)
	{
		return LIST_ERR_EMPTY;
	}
	p = malloc(sizeof(struct Node));
	if (p == NULL)
	{
		return LIST_ERR_MALLOC;
	}
	p->data = input_head;
	p->next = *head;
	*head = p;
	return LIST_OK;
}

//正常插入+结尾插入
int insert_after(struct Node* p, struct Node** tail, int input)
{
	if (p == NULL)
	{
		return LIST_ERR_NOT_FOUND;
	}
	if (tail == NULL)
	{
		return LIST_ERR_NULL;
	}
	if (*tail == NULL)
	{
		return LIST_ERR_EMPTY;
	}
	struct Node* new_node = NULL;
	new_node = malloc(sizeof(struct Node));
	if (new_node == NULL)
	{
		return LIST_ERR_MALLOC;
	}
	new_node->data = input;
	new_node->next = p->next;
	p->next = new_node;

	if (p == *tail)
	{
		*tail = new_node;
	}
	return LIST_OK;
}


//更新当前节点
int update(struct Node* p, int new_data)
{
	if (p == NULL)
	{
		return LIST_ERR_NOT_FOUND;
	}
	p->data = new_data;
	return LIST_OK;
}

//删除函数
int delete_node(struct Node** head, struct Node** tail, struct Node* p)
{
	if (p == NULL)
	{
		return LIST_ERR_NOT_FOUND;
	}
	if (head == NULL || tail == NULL)
	{
		return LIST_ERR_NULL;
	}
	if (*head == NULL || *tail == NULL)
	{
		return LIST_ERR_EMPTY;
	}
	if (p == *head)
	{
		*head = p->next;
		free(p);
		if (*head == NULL)
		{
			*tail = NULL;
		}
		return LIST_OK;
	}
	struct Node* temp = p;
	struct Node* prev = *head;
	p = (*head)->next;
	while (p != NULL)
	{
		if (p == temp)
		{
			prev->next = p->next;
			if (p == *tail)
			{
				*tail = prev;
				prev->next = NULL;
			}
			free(p);
			return LIST_OK;
		}
		prev = p;
		p = p->next;
	}
	return LIST_ERR_NOT_FOUND;
}
//释放全部链表
int free_list(struct Node** head, struct Node** tail)
{
	if (head == NULL || tail == NULL)
	{
		return LIST_ERR_NULL;
	}
	if (*head == NULL || *tail == NULL)
	{
		return LIST_ERR_EMPTY;
	}
	struct Node* p = *head;
	struct Node* temp = NULL;
	while (p != NULL)
	{
		temp = p->next;
		free(p);
		p = temp;
	}
	*head = NULL;
	*tail = NULL;
	return LIST_OK;
}

//输出链表
int print_list(struct Node* head)
{
	if (head == NULL)
	{
		return LIST_ERR_EMPTY;
	}
	struct Node* p = head;
	while (p != NULL)
	{
		printf("%d  ", p->data);
		p = p->next;
	}
	printf("\n");
	return LIST_OK;
}

//错误码封装
void print_error(int ret)
{
	switch (ret)
	{
	case LIST_OK:
		printf("操作成功\n");
		break;
	case LIST_ERR_NULL:
		printf("参数为空\n");
		break;
	case LIST_ERR_EMPTY:
		printf("链表为空\n");
		break;
	case LIST_ERR_MALLOC:
		printf("内存申请失败\n");
		break;
	case LIST_ERR_NOT_FOUND:
		printf("没找到目标节点\n");
		break;
	case LIST_ERR_INVALID:
		printf("非法操作\n");
		break;
	default:
		printf("未知错误\n");
		break;
	}
}