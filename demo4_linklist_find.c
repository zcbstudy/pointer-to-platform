#include <stdio.h> 
#include <stdlib.h>
//创建链表函数
struct Node
{
	int data;
	struct Node* next;
};
//创建查找函数
struct Node* find(struct Node* head, int a)
{
	struct Node* p = head;
	while (p != NULL)
	{
		if (p->data == a)
			break;
		else p = p->next;
	}
	return p;
}
int main()
{
	struct Node* new = NULL;
	struct Node* head = NULL;
	struct Node* tail = NULL;
	int a;struct Node* p;int b;struct Node* prev;
	//输入数据， - 1结束
	while (1)
	{
		scanf_s("%d", &a);
		if (a == -1)
		{
			break;
		}
		new = malloc(sizeof(struct Node));

		new->next = NULL;
		if (head == NULL)
		{
			head = new;
			tail = new;
		}
		else
		{
			tail->next = new;
			tail = new;
		}
		new->data = a;
	}
	//查找需要的数据
	p=find(head, 3);
	while (1)
	{
		if (p != NULL)
		{
			printf("\n%d\n", p->data);
			break;
		}
		else
		{
			printf("flase");
			break;
		}
		p = p->next;
	}
	/*
	#遍历数组
	p = head;
	while (p != NULL)
	{
		printf("%d\n", p->data);
		p = p->next;
	}

	p = head;prev = NULL;

	#加入删除功能
	scanf_s("%d", &b);
	while (p != NULL)
	{
		if (p->data == b)
		{
			if (p == head)
			{
				head = p->next;
			}
			else
			{
				prev->next = p->next;
			}
			if (p == tail)
			{
				tail = prev;
			}
			free(p);
			p = NULL;
			break;
		}
		prev = p;
		p = p->next;
	}
	#打印修改后的链表
	p = head;
	while (p != NULL)
	{
		printf("%d\n", p->data);
		p = p->next;
	}
	#释放内存
	p = head;
	while (p != NULL)
	{
		struct Node* temp = p;
		p = p->next;
		free(temp);
	}
	head = NULL;
	tail = NULL;
	return 0;*/
}