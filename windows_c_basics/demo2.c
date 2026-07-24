#include <stdio.h> 
#include <stdlib.h>
struct Node
{
	int data;
	struct Node* next;
};
int main()
{
	struct Node* new;
	struct Node* head;
	struct Node* tail;
	head = malloc(sizeof(struct Node));
	tail = malloc(sizeof(struct Node));
	head->next = tail;
	head->data = 0;
	tail->data = 1;
	struct Node* prev;int a;struct Node* p;
	prev = head;
	while (1)
	{
		scanf_s("%d", &a);
		if (a == -1)
		{
			break;
		}
		new = malloc(sizeof(struct Node));
		new->data = a;
		new->next = tail;
		head->next = new;
		prev->next = new;
		prev = new; 
	}
	p = head;
	while (p != NULL)
	{
		printf("%d\n", p->data);
		p = p->next;
	}
	return 0;
}
