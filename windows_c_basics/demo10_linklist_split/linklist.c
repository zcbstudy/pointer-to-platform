#include <stdio.h>
#include <stdlib.h>
#include "linklist.h"

//创建链表
int create(struct Node** head, struct Node** tail, int input)
{
	if (head == NULL || tail == NULL)
	{
		return -1;
	}

	if (*head != NULL || *tail != NULL)
	{
		return -1;
	}

	struct Node* new_node = malloc(sizeof(struct Node));
	if (new_node == NULL)
	{
		return -1;
	}

	new_node->data = input;
	new_node->next = NULL;

	*head = new_node;
	*tail = new_node;

	return 0;
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
	if (head == NULL || *head == NULL)
	{
		return -1;
	}
	p = malloc(sizeof(struct Node));
	if (p == NULL)
	{
		return -1;
	}
	p->data = input_head;
	p->next = *head;
	*head = p;
	return 0;
}

//正常插入+结尾插入
int insert_after(struct Node* p, struct Node** tail, int input)
{
	if (p == NULL || tail == NULL || *tail == NULL)
	{
		return -1;
	}
	struct Node* new_node = NULL;
	new_node = malloc(sizeof(struct Node));
	if (new_node == NULL)
	{
		return -1;
	}
	new_node->data = input;
	new_node->next = p->next;
	p->next = new_node;

	if (p == *tail)
	{
		*tail = new_node;
	}
	return 0;
}


//更新当前节点
int update(struct Node* p, int new_data)
{
	if (p == NULL)
	{
		return -1;
	}
	p->data = new_data;
	return 0;
}

//删除函数
int delete_node(struct Node** head, struct Node** tail, struct Node* p)
{
	if (p == NULL || head == NULL || tail == NULL || *head == NULL || *tail == NULL)
	{
		return -1;
	}
	if (p == *head)
	{
		*head = p->next;
		free(p);
		if (*head == NULL)
		{
			*tail = NULL;
		}
		return 0;
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
			return 0;
		}
		prev = p;
		p = p->next;
	}
	return -1;
}
//释放全部链表
int free_list(struct Node** head, struct Node** tail)
{
	if (head == NULL || tail == NULL || *head == NULL || *tail == NULL)
	{
		return -1;
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
	return 0;
}

//输出链表
int print_list(struct Node* head)
{
	if (head == NULL)
	{
		return -1;
	}
	struct Node* p = head;
	while (p != NULL)
	{
		printf("%d  ", p->data);
		p = p->next;
	}
	printf("\n");
	return 0;
}
