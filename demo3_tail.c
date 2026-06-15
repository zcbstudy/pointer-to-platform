#include <stdio.h> 
#include <stdlib.h>
struct Node
{
	int data;
	struct Node* next;
};
int main()
{
	struct Node* new=NULL;
	struct Node* head=NULL;
	struct Node* tail=NULL;
	int a;struct Node* p;int b;struct Node* prev;
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
	p = head;
	while (p != NULL)
	{
		printf("%d\n", p->data);
		p = p->next;
	}

	p = head;prev = NULL;
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

		p = head;
		while (p != NULL)
		{
			printf("%d\n", p->data);
			p = p->next;
		}
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