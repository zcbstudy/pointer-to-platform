#include <stdio.h>
#include <stdlib.h>

//创建链表函数
struct Node
{
	int data;
	struct Node* next;
};

//创建链表
void create(struct Node** head, struct Node** tail,int input)
{
	struct Node* new_node = malloc(sizeof(struct Node));
	new_node->data = input;
	new_node->next = NULL;
	if (*head == NULL && *tail == NULL)
	{
		*head = new_node;
		*tail = new_node;
	}
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
	if (head==NULL||*head==NULL)
	{
		return -1;
	}
	p = malloc(sizeof(struct Node));
	if (p == NULL)
	{
		free(p);
		return -1;
	}
	p->data = input_head;
	p->next = *head;
	*head = p;
	return 0;
}

//正常插入+结尾插入
int insert_after(struct Node* p,struct Node**tail, int input)
{
	if (p == NULL||tail==NULL||*tail==NULL)
	{
		return -1;
	}
	struct Node* new_node = NULL;
	new_node = malloc(sizeof(struct Node));
	if (new_node == NULL)
	{
		free(new_node);
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
	struct Node* p = *head;struct Node* temp = NULL;
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
	return 0;
}
int main()
{
	struct Node* head = NULL;
	struct Node* tail = NULL;
	struct Node* p = NULL;
	int ret = 0;
	int input_first,input_temp, input_find,input_insert_first, input_insert, input_update_first, input_update;
	int input_delete_first, input_delete, input_free_first;
	printf("创建链表，请输入链表第一个节点");
	scanf_s("%d", &input_first);
	create(&head, &tail, input_first);
	printf("开始插入，输入-1结束\n");
	scanf_s("%d", &input_temp);
	while (input_temp != -1)
	{
		printf("请输入要查找的数字：");
		scanf_s("%d", &input_find);
		p = find(head, input_find);
		printf("请选择插入位置\n 1：插入当前链表最前面 2：插入每个查找到的数字后面");
		scanf_s("%d", &input_insert_first);
		printf("\n 输入插入的数字：");
		scanf_s("%d", &input_insert);
		if (input_insert_first == 1)
		{
			ret = insert_head(&head, input_insert);
			if (ret == 0)
			{
				printf("头插成功\n");
			}
			else
			{
				printf("头插失败\n");
			}
		}
		else if (input_insert_first == 2)
		{
			if (p == NULL)
			{
				printf("没找到该节点，不能后插\n");
			}
			else
			{
				ret = insert_after(p, &tail, input_insert);

				if (ret == 0)
				{
					printf("后插成功\n");
				}
				else
				{
					printf("后插失败\n");
				}
			}
			print_list(head);
		}
		printf("继续插入请输入任意数字，结束请输入 -1：");
		scanf_s("%d", &input_temp);
	}
	printf("开始更改，输入-1结束");
	scanf_s("%d", &input_update_first);
	while (input_update_first != -1) 
	{
		printf("请输入要查找的数字：");
		scanf_s("%d", &input_find);
		p = find(head, input_find);
		if (p == NULL)
		{
			printf("没找到该节点，不能修改\n");
		}
		else
		{
			printf("请输入要替换为的数字：");
			scanf_s("%d", &input_update);
			ret = update(p, input_update);
			if (ret == 0)
			{
				printf("修改成功\n");
			}
			else
			{
				printf("修改失败\n");
			}
			print_list(head);
		}
		printf("继续修改请输入任意数字，结束请输入 -1：");
		scanf_s("%d", &input_update_first);
	}
	printf("开始删除，输入-1结束");
	scanf_s("%d", &input_delete_first);
	while (input_delete_first != -1)
	{
		printf("请输入要查找的数字：");
		scanf_s("%d", &input_find);
		p = find(head, input_find);
		if (p == NULL)
		{
			printf("没找到该节点，不能删除\n");
		}
		else
		{
			printf("开始删除查找到的数字：");
			ret = delete_node(&head,&tail,p);
			if (ret == 0)
			{
				printf("删除成功\n");
			}
			else
			{
				printf("删除失败\n");
			}
			print_list(head);
		}
		printf("继续删除请输入任意数字，结束请输入 -1：");
		scanf_s("%d", &input_delete_first);
	}
	do
	{
		printf("按 9 释放全部链表：");
		scanf_s("%d", &input_free_first);

		if (input_free_first != 9)
		{
			printf("输入错误，请重新输入\n");
		}

	} while (input_free_first != 9);

	ret = free_list(&head, &tail);

	if (ret == 0)
	{
		printf("释放成功\n");
	}
	else
	{
		printf("释放失败\n");
	}
	return 0;
}