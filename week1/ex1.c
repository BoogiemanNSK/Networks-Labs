#include <stdio.h>
#include <string.h>

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
	char buf[20];
	int fds[2];
	pipe(fds);
	
	int pid = fork();
	if (pid > 0) {
		// Client
		close(fds[0]);

		while (1) {
			fgets(buf, 100, stdin);
			write(fds[1], buf, 20);
		}
	} else {
		// Server
		close(fds[1]);
		
		while (1) {
			read(fds[0], buf, 20);
			struct stack *st = NULL;
			
			if (strcmp(buf, "peek") == 0) {
				int p = peek(st);
				printf("Picked %d from stack.\n", p);
			} else if (strcmp(buf, "pop") == 0) {
				pop(st);
				printf("Popped stack head element.\n");
			} else if (strcmp(buf, "empty") == 0) {
				if (empty(st)) {
					printf("Stack is empty.\n");
				} else {
					printf("Stack is not empty.\n");
				}
			} else if (strcmp(buf, "display") == 0) {
				printf("Stack elements: ");
				display(st);
				printf("\n");
			} else if (strcmp(buf, "create") == 0) {
				if (st != NULL) {
					while (!empty(st)) {
						pop(st);
					}
					free(st);
				}
				st = create();
				printf("Created new stack.");
			} else if (strcmp(buf, "stack_size") == 0) {
				int n = stack_size(st);
				printf("Current stack size is %d.\n", n);
			} else {
				int data = 0, i = 5;
				while (buf[i] != '\0') {
					data *= 10;
					data += (int)(buf[i] - 48);
					i++;
				}
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

void pop(struct stack st*) {
	if (empty(st)) { return; }
	
	struct stack *temp_head = st->head;
	st->head = temp->next;
	free(temp);
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