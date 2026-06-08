#include <stdio.h> 
#include <stdlib.h>
struct Node {
	int data;
	struct Node* next;
};
int main() {
	struct Node* node1;
	struct Node* node2;
	struct Node* node3;	
	struct Node* node4;
	node1 = malloc(sizeof(struct Node));
	node2 = malloc(sizeof(struct Node));
	node3 = malloc(sizeof(struct Node));
	node4 = malloc(sizeof(struct Node));
	node1->data = 1;
	node2->data = 2;
	node3->data = 3;
	node4->data = 4;
	node1->next = node4;
	node4->next = node2;
	node2->next = node3;
	node3->next = NULL;
	struct Node* p;
	p = node1;
	while (p != NULL) {
		printf("%d\n", p->data);
		p = p->next;
		
	}
	return 0;

}