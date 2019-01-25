#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

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
int stack_size(struct stack *st);
void intro();

int main() {
	intro();
	
	char buf[20];
	int fds[2];
	pipe(fds);
	
	int pid = fork();
	if (pid > 0) {
		// Client
		close(fds[0]);

		while (1) {
			fgets(buf, 16, stdin);
			write(fds[1], buf, 20);
		}
	} else {
		// Server
		close(fds[1]);
		struct stack *st = NULL;
		
		while (1) {
			read(fds[0], buf, 20);

			if (strncmp(buf, "peek", 4) == 0) {
				int p = peek(st);
				printf("Stack top element is %d.\n\n", p);
			} else if (strncmp(buf, "pop", 3) == 0) {
				pop(st);
				printf("Popped stack head element.\n\n");
			} else if (strncmp(buf, "empty", 5) == 0) {
				if (empty(st)) {
					printf("Stack is empty.\n\n");
				} else {
					printf("Stack is not empty.\n\n");
				}
			} else if (strncmp(buf, "display", 7) == 0) {
				printf("Stack elements: ");
				display(st);
				printf("\n\n");
			} else if (strncmp(buf, "create", 6) == 0) {
				if (st != NULL) {
					while (!empty(st)) {
						pop(st);
					}
					free(st);
				}
				st = create();
				printf("Created new stack.\n\n");
			} else if (strncmp(buf, "stack_size", 10) == 0) {
				int n = stack_size(st);
				printf("Current stack size is %d.\n\n", n);
			} else if (strncmp(buf, "push", 4) == 0) {
				int data = 0, i = 5;
				while (buf[i] != '\n') {
					data *= 10;
					data += (int)(buf[i] - 48);
					i++;
				}
				push(st, data);
				printf("Pushed %d to the stack.\n\n", data);
			} else {
				printf("Invalid input.\n\n");
			}		
		}
	}
	
	return 0;
}

int peek(struct stack *st) {
	return st->head->data;
}

void push(struct stack *st, int data) {
	struct node *new_head = (struct node*)malloc(sizeof(struct node));
	
	new_head->data = data;
	new_head->next = st->head;
	
	st->head = new_head;
	st->size++;
}

void pop(struct stack *st) {
	if (empty(st)) { return; }
	
	struct node *temp_head = st->head;
	st->head = temp_head->next;
	free(temp_head);
	st->size--;
}

int empty(struct stack *st) {
	return (st->size == 0 ? 1 : 0);
}

void display(struct stack *st) {
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

int stack_size(struct stack *st) {
	return st->size;
}

void intro() {
	system("clear");

	printf("Faraday Stack v1.0\n");
	printf("List of availible commands:\n");
	printf("create      -- establish new empty stack\n");
	printf("push [data] -- adds [data] to the top of stack\n");
	printf("pop         -- removes stack head element\n");
	printf("peek        -- shows stack head element\n");
	printf("display     -- shows the whole stack\n");
	printf("empty       -- checks if the stack is empty\n");
	printf("stack_size  -- shows current stack size\n\n");
}
