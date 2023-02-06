#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int MAX_BUFF_SIZE = 100;
char HEADER_DIVIDER[4] = "[:]";
char MESSAGE_DIVIDER[4] = "[:]";

void *reverse(char *s)
{
    int i, j;
    char c;
    for (i = 0, j = strlen(s)-1; i<j; i++, j--) {
        c = s[i];
        s[i] = s[j];
        s[j] = c;
    }
    return s;
} 

char *itoa(int n){
    int i, sign;
    char *s = NULL;
    if ((sign = n) < 0)  /* record sign */
        n = -n;          /* make n positive */
    i = 0;
    do {       /* generate digits in reverse order */
        s = realloc(s, sizeof(char));
        s[i++] = n % 10 + '0';   /* get next digit */
    } while ((n /= 10) > 0);     /* delete it */
    if (sign < 0)
        s[i++] = '-';
    s[i] = '\0';
    s = reverse(s);
    return s;
}

char *addHeader(char *info, int header){
    char *message = calloc(MAX_BUFF_SIZE, sizeof(char));
    strcpy(message, itoa(header));
    if (info != NULL) {
        strcat(message, HEADER_DIVIDER);
        strcat(message, info);
        info = NULL;
    }
    return message;
}

char *appendInfo(char *current_info, char *new_info){
    if (strlen(current_info) == 0 || current_info == NULL) {
        char *info = calloc(1+strlen(new_info), sizeof(char));
        strcpy(info, new_info);
        if (new_info != NULL) new_info = NULL;
        return info;
    } else {
        char *message = calloc(MAX_BUFF_SIZE, sizeof(char));
        strcpy(message, current_info);
        strcat(message, MESSAGE_DIVIDER);
        strcat(message, new_info);
        if (current_info != NULL) current_info = NULL;
        if (new_info != NULL) new_info = NULL;
        return message;
    }
}

char *getInfo(char *message){
    char *token;
    char *info = calloc(MAX_BUFF_SIZE, sizeof(char));
    token = strtok(message, HEADER_DIVIDER);
    strcpy(info, token + strlen(token) + strlen(HEADER_DIVIDER));
    return info;
}

char *afterInfo(char *info, int is_first){
    char *token;
    char *value = calloc(30, sizeof(char));
    if (is_first) {
        char *first_info = calloc(MAX_BUFF_SIZE, sizeof(char));
        strcpy(first_info, info);
        token = strtok(first_info, MESSAGE_DIVIDER);
        strcpy(value, token);
    } else {
        token = strtok(NULL, MESSAGE_DIVIDER);
        strcpy(value, token);
    }
    return value;
}

char *nextInfo(char *info, int is_first){
    char *token;
    char *value = calloc(100, sizeof(char));
    if (is_first) {
        char *first_info = calloc(MAX_BUFF_SIZE, sizeof(char));
        strcpy(first_info, info);
        token = strtok(first_info, MESSAGE_DIVIDER);
        strcpy(value, token);
        free(first_info);
    } else {
        token = strtok(NULL, MESSAGE_DIVIDER);
        strcpy(value, token);
    }
    return value;
}

int main(){
    // char a[100] = "100[:]Hello[:]Hello";
    // char *token;
    // char *tmp;
    // char header_divider[4] = "[:]";
    // char message_divider[4] = "[&]";
    // token = strtok(a, header_divider);
    // printf("%s\n", token);
    // tmp = token + strlen(token) + strlen(header_divider);
    // token = strtok(tmp, message_divider);
    // // token = strtok(NULL, "[&]");
    // printf("%s\n", token);
    char *a = calloc(5, sizeof(char));
    char *new_a = appendInfo("nothing to do", "new thing to do");
    new_a = addHeader("Slow pock", 100);
    new_a = appendInfo(new_a, "ohno");
    new_a = appendInfo(new_a, "nothing");
    new_a = appendInfo(new_a, "for the love of god");
    printf("%s\n", new_a);
    new_a = getInfo(new_a);
    char *first = afterInfo(new_a, 1);
    char *oh = afterInfo(new_a, 0);
    printf("%s\n", afterInfo(new_a, 0));
    printf("%s\n", afterInfo(new_a, 0));
    printf("%s\n", new_a);
    new_a = getInfo(new_a);
    first = afterInfo(new_a, 1);
    printf("%s\n", first);
}