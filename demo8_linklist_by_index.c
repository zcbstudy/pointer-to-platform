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
	struct Node* p = head;int count = 0;
	while (p != NULL)
	{
		count++;
		p = p->next;
	}
	return count;
}

int main()
{
	struct Node* new_node = NULL;
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
		new_node = malloc(sizeof(struct Node));

		new_node->next = NULL;
		if (head == NULL)
		{
			head = new_node;
			tail = new_node;
		}
		else
		{
			tail->next = new_node;
			tail = new_node;
		}
		new_node->data = a;
	}

	//遍历数组查数
	int x = count_list(head);
	printf("当前链表长度为 %d", x);

	//遍历链表找到用户规定插入的位置
	
	printf("输入插入的位置");
	scanf_s("%d", &a);a = a - 1;
	printf("输入加入的值");
	scanf_s("%d", &put);
	if (a<0 || a>x)
	{
		printf("flase");
	}
	else if (a == 0)
	{
		new_node = malloc(sizeof(struct Node));
		new_node->next = head;
		new_node->data = put;
		head = new_node;
		if (tail == NULL)
		{
			tail = new_node;
		}
	}
	else if (a == x)
	{
		inseret(tail, put);
		tail = tail->next;
	}
	else
	{
		p = head;
		for (int i = 1;i < a;i++)
		{
			p = p->next;
		}
		inseret(p, put);
	}


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