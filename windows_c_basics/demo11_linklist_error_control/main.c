#include <stdio.h>
#include "linklist.h"
int main()
{
	struct Node* head = NULL;
	struct Node* tail = NULL;
	struct Node* p = NULL;
	int ret = 0;
	int input_first, input_temp, input_find, input_insert_first, input_insert, input_update_first, input_update;
	int input_delete_first,input_free_first;
	printf("创建链表，请输入链表第一个节点");
	scanf_s("%d", &input_first);
	ret = create(&head, &tail, input_first);
	if (ret != LIST_OK)
	{
		printf("创建链表失败\n");
		print_error(ret);
		return -1;
	}
	printf("开始插入，输入-1结束\n");
	scanf_s("%d", &input_temp);
	while (input_temp != -1)
	{
		printf("请输入要查找的数字：");
		scanf_s("%d", &input_find);
		p = find(head, input_find);
		if (p == NULL)
		{
			printf("没找到该节点，不能修改\n");
		}
		printf("请选择插入位置\n 1：插入当前链表最前面 2：插入每个查找到的数字后面");
		scanf_s("%d", &input_insert_first);
		printf("\n 输入插入的数字：");
		scanf_s("%d", &input_insert);
		if (input_insert_first == 1)
		{
			ret = insert_head(&head, input_insert);
			if (ret == LIST_OK)
			{
				printf("头插成功\n");
				print_list(head);
			}
			else
			{
				printf("头插失败\n");
				print_error(ret);
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

				if (ret == LIST_OK)
				{
					printf("后插成功\n");
				}
				else
				{
					printf("后插失败\n");
					print_error(ret);
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
			if (ret == LIST_OK)
			{
				printf("修改成功\n");
			}
			else
			{
				printf("修改失败\n");
				print_error(ret);
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
			ret = delete_node(&head, &tail, p);
			if (ret == LIST_OK)
			{
				printf("删除成功\n");
				ret = print_list(head);
				if (ret == LIST_ERR_EMPTY)
				{
					printf("当前链表已为空\n");
				}
			}
			else
			{
				printf("删除失败\n");
				print_error(ret);
			}
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

	if (ret == LIST_OK)
	{
		printf("释放成功\n");
	}
	else if (ret == LIST_ERR_EMPTY)
	{
		printf("链表已经为空，无需释放\n");
	}
	else
	{
		printf("释放失败\n");
		print_error(ret);
	}
	return 0;
}