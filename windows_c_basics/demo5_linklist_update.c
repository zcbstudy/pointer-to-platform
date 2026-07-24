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
//创建修改函数
void update(struct Node* p, int new)
{
		p->data = new;
}
int main()
{
	struct Node* new = NULL;
	struct Node* head = NULL;
	struct Node* tail = NULL;
	int a;struct Node* p=NULL;int b;
	//输入数据， - 1结束
	printf("输入数组元素");
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
	printf("输入旧值-1出循环\n");
	while (1)
	{
		printf("输入旧值");
		scanf_s("%d", &a);
		if (a != -1)
			p = find(head, a);
		else break;
		
		if (p != NULL)
		{
			printf("输入新值");
			scanf_s("%d", &b);
			update(p, b);
		}else
			printf("flase");	
	}
	//遍历数组
	p = head;
	while (p != NULL)
	{
		printf("%d\n", p->data);
		p = p->next;
	}

	p = head;
	
	//释放内存
	p = head;
	while (p != NULL)
	{
		struct Node* temp = p;
		p = p->next;
		free(temp);
	}
	head = NULL;
	tail = NULL;
	return 0;
	
}