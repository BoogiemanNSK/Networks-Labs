#include <stdio.h>

struct node {
	struct node *next;
	int data;
};

struct stack {
	struct node *head;
	int size;
};

int peek(struct stack *st);
void push(struct stack *st, int data);
void pop(struct stack *st);
int empty(struct stack *st);
void display(struct stack *st);
struct stack* create();
void stack_size(struct stack *st);

int main() {
	int fds[2];
	pipe(fds);
	int pid = fork();
	if (pid > 0) {
		// Client
		close(fds[0]);

	} else {
		// Server
		close(fds[1]);

	}
	return 0;
}

int peek(struct stack *st) {
	return st.head->data;
}

void push(struct stack *st, int data) {
	struct node *tail = st->head;

	if (tail == NULL) {
		tail = (struct node*)malloc(sizeof(struct node));
	} else {
		while (tail->next != NULL) {
			tail = tail->next;
		}
		tail->next = (struct node*)malloc(sizeof(struct node));
		tail = tail->next;
	}

	tail->data = data;
	tail->next = NULL;
}

void pop(struct stack st*) {
	struct stack *temp_head = st->head;
	int temp_data = temp_head->data;
	
	st->head = temp->next;
	free(temp);
	printf("%d\n", temp_data);
}

int empty(struct stack st*) {
	return (st->size == 0 ? 1 : 0);
}

void display(struct stack st*) {
	if (st->head != NULL) {
		struct node* temp = st->head;
		printf("%d", temp->data);
		
		while (temp->next != NULL) {
			temp = temp->next;
			printf(" -> %d", temp->data);
		}
	}
}

struct stack* create() {
	struct stack *st = (struct stack*)malloc(sizeof(struct stack));
	st->head = NULL;
	st->size = 0;
	return st;
}

void stack_size(struct stack st*) {
	printf("%d\n", st->size);
}