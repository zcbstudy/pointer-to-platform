#ifndef LINKLIST_H
#define LINKLIST_H
#define LIST_OK 0
#define LIST_ERR_NULL -1
#define LIST_ERR_EMPTY -2
#define LIST_ERR_MALLOC -3
#define LIST_ERR_NOT_FOUND -4
#define LIST_ERR_INVALID -5
/*LIST_OK            成功
LIST_ERR_NULL      参数为空，比如 head == NULL
LIST_ERR_EMPTY     链表为空，比如 *head == NULL
LIST_ERR_MALLOC    malloc 失败
LIST_ERR_NOT_FOUND 没找到节点
LIST_ERR_INVALID   非法操作或非法选择*/

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