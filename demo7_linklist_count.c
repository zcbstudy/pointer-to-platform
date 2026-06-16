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

//创建插入函数
void inseret(struct Node* p, int put)
{
	struct Node* new_node = NULL;
	new_node = malloc(sizeof(struct Node));
	new_node->data = put;
	new_node->next = p->next;
	p->next = new_node;
}

//创建计数函数
int count_list(struct Node* head)
{
	struct Node* p = head;int count=0;
	while (p != NULL)
	{
			count++;
			p = p->next;
	}
	return count;
}

int main()
{
	struct Node* new = NULL;
	struct Node* head = NULL;
	struct Node* tail = NULL;
	int a;struct Node* p = NULL;int put;

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

	//查找并插入
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
			scanf_s("%d", &put);
			if (p == tail)
			{
				inseret(p, put);
				tail = tail->next;
			}
			else inseret(p, put);
		}
		else
			printf("flase");
	}

	//遍历数组查数
	int x=count_list(head);
	printf("%d", x);

	//遍历数组
	p = head;
	while (p != NULL)
	{
		printf("%d\n", p->data);
		p = p->next;
	}

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