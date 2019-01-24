#include <stdio.h>

struct node {
	struct node *next;
	int data;
}

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
