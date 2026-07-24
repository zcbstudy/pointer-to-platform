#ifndef LINKLIST_H
#define LINKLIST_H

struct Node
{
	int data;
	struct Node* next;
};
int create(struct Node** head, struct Node** tail, int input);
struct Node* find(struct Node* head, int find_data);
int insert_head(struct Node** head, int input_head);
int insert_after(struct Node* p, struct Node** tail, int input);
int update(struct Node* p, int new_data);
int delete_node(struct Node** head, struct Node** tail, struct Node* p);
int free_list(struct Node** head, struct Node** tail);
int print_list(struct Node* head);

#endif