//Huffman code

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

struct node{
	int value;
	char ch;
	struct node *lson, *rson;
};

typedef struct node Node;

typedef struct queue{
	Node * n;
	struct queue * next;
} Queue;

Queue * head;

Queue * createQueue(){
	Queue * q = malloc(sizeof(Queue));
	q -> next = NULL;
	q -> n = NULL;
	return q;
}

void push(Node * n){
	//printf("Push: %d \n", n->value);
	if(head == NULL){
		head = createQueue();
	}
	if(head->n == NULL) {
		head->n = n;
		return;
	}
	Queue *q = head, * q_next = createQueue();
	q_next->n = n;
	while(q->next != NULL){
		if(n->value <= q->next->n->value) break;
		q = q->next;
	}
	if(q == head && n->value < q->n->value){
		q_next -> next = q;
		head = q_next;
	}
	q_next -> next = q->next;
	q->next = q_next;
}

int isEmpty(){
	return head==NULL ? 1:0;
}

int isAlone(){
	return head->next == NULL ? 1:0;
}

Node * pop(){
	Node * n = head -> n;
	Queue * h = head;
	head = head -> next;
	if(head != NULL)
		free(h);
	return n;
}

Node * peek(){
	return head -> n;
}

//Find if a character is in a string and that characters position
int contains(char, char *, int);
void quickSort();
void count();
int partition();
Node * huffmanTree();
void printTree();
void printQueue();



int reps[256] = {};
char chars[256] = {EOF};
int uniq = 0;

Node * nodes[256];

Node * createNode(){
	Node * n = malloc(sizeof(Node));
	n -> lson = NULL;
	n -> rson = NULL;
	return n;
}

int main(int argc, char * argv[]){
	char ch;
	char str[40960];
	int i = 0;
	while((ch=getc(stdin))!=EOF){
		str[i++] = ch;
	}
	int size = strlen(str);
//	quickSort(argv[1], 0, size);
//	argv[1][0] = ' ';
//	printf("%s\n%d\n", argv[1], size);
	huffmanTree(str,size);
	return 0;
}

void count(char arr[], int size){
	int i = 0, pos = -1;
	for(i=0;i<size;i++) {
		pos = contains(arr[i], chars, uniq);
		if(pos > 0) {
			reps[pos]++;
		} else {
			chars[uniq] = arr[i];
			reps[uniq]++;
			uniq++; 
		}
	}
}

int contains(char ch, char arr[], int size){
	int i=0;
	for(i=0; i<size; i++){
		if(ch==arr[i]) return i;
	}
	return -1;
}

//*-----------------------------QUICKSORT ALGORITHM FOR TESTING-----------------------------------*//
//-------------------------------------------------------------------------------------------------//
void quickSort(int l, int r)
{
   int j;

   if( l < r ) 
   {
   	// divide and conquer
        j = partition(l, r);
       quickSort(l, j-1);
       quickSort(j+1, r);
   }
	
}

int partition(int l, int r) {
   int i, j, pivot = reps[l], t;
   char tmp;
   i = l; j = r+1;
   printf("%d %d %d\n", l, j, r);
		
   while(1)
   {
   	do ++i; while( reps[i] <= pivot && i <= r );
   	do --j; while( reps[j] > pivot );
   	if( i >= j ) break;
   	t = reps[i]; reps[i] = reps[j]; reps[j] = t;
   	tmp = chars[i]; chars[i] = chars[j]; chars[j] = tmp;
   }
   t = reps[l]; reps[l] = reps[j]; reps[j] = t;
   tmp = chars[l]; chars[l] = chars[j]; chars[j] = tmp;
   return j;
}
//------------------------------------------------------------------------------------------------//
//*---------------------------------------END OF QUICKSORT---------------------------------------*//

void printNode(Node * n){
	printf("v-%d | c-%c | lson-%d | rson-%d\n", n->value, n->ch, (int)n->lson, (int)n->rson);
}

Node * huffmanTree(char argv[], int size){
	int i = 0;
	count(argv, size);
	//quickSort(0,uniq-1);
	Node * n;
	head = createQueue();
	printf("Text: %d\nUnique: %d\n", size,uniq);
	for(i=0;i<uniq;i++){
		n = createNode();
		n -> value = reps[i];
		n -> ch = chars[i];
		push(n);
		//printf("%c : %d\n", chars[i], reps[i]);
	}
	printQueue();
	i = 0;
	Node * n1, * n2;
	while(!isAlone()){
		n1 = pop();
		if(n1 == NULL) break;
		n2 = pop();
		if(n2 == NULL) break;
		n = createNode();
		n -> lson = n1;
		n -> rson = n2;
		n -> value = n1 -> value + n2 -> value;
		push(n);
	}
	printf("\n\n");
	char str[1024] = {EOF};
	printTree(n,str);
	return n;
}

int end = 0;

void printTree(Node * n, char str[]){
	if(n-> lson != NULL) {
		str[end] = '0';
		end++;
		printTree(n->lson, str);
	}
	if(n->rson != NULL) {
		str[end] = '1';
		end++;
		printTree(n->rson, str);
	} 
	if(n->lson==NULL && n->rson==NULL) {
		str[end] = '\0';
		printf("%s : %c : %d bits\n", str, n->ch, end);
	}
	end--;
}

void printQueue(){
	Queue * q =  head;
	int i = 0;
	while(q != NULL){
		printf("Q: %d %c %d\n", i++, q->n==NULL ? ' ':q->n->ch, q->n==NULL ? 0:q->n->value);
		q = q->next;
	}
}

