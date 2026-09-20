#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "cJSON.h"

#include "json.h"

Connection connections[MAX_CONNECTIONS];

int connection_count = 0;

/*-----------------------------------------*/
/* Convert n1-n8 to 0-7                    */
/*-----------------------------------------*/

int node_to_index(const char *node)
{
    if(node == NULL)
        return -1;

    if(node[0] != 'n' && node[0] != 'N')
        return -1;

    int index = atoi(node + 1) ;

    if(index < 0 || index >= NUM_NODES)
        return -1;

    return index;
}

/*-----------------------------------------*/
/* Parse JSON                              */
/*-----------------------------------------*/

void parse_json(const char *json_string)
{
    cJSON *root = cJSON_Parse(json_string);

    if(root == NULL)
    {
        printf("JSON Parse Error\n");
        return;
    }

    cJSON *array =
        cJSON_GetObjectItem(root,"connections");

    if(array == NULL)
    {
        printf("Connections not found\n");
        cJSON_Delete(root);
        return;
    }

    connection_count = cJSON_GetArraySize(array);

    for(int i=0;i<connection_count;i++)
    {
        cJSON *item =
            cJSON_GetArrayItem(array,i);

        cJSON *from =
            cJSON_GetObjectItem(item,"from");

        cJSON *to =
            cJSON_GetObjectItem(item,"to");

        cJSON *type =
            cJSON_GetObjectItem(item,"type");

        cJSON *res =
            cJSON_GetObjectItem(item,"expected_resistance");

        cJSON *state =
            cJSON_GetObjectItem(item,"state");

        connections[i].from =
            node_to_index(from->valuestring);

        connections[i].to =
            node_to_index(to->valuestring);

        strcpy(connections[i].type,
               type->valuestring);

        if(res != NULL)
        {
            connections[i].expected_resistance =
                res->valueint;
        }
        else
        {
            connections[i].expected_resistance = -1;
        }

        if(state != NULL)
        {
            strcpy(connections[i].state,
                   state->valuestring);
        }
        else
        {
            strcpy(connections[i].state,
                   "NONE");
        }
    }

    cJSON_Delete(root);
}

/*-----------------------------------------*/
/* Print Parsed Connections                */
/*-----------------------------------------*/

void print_connections(void)
{
    printf("\n");
    printf("=======================================\n");
    printf("Parsed Connections\n");
    printf("=======================================\n");

    for(int i=0;i<connection_count;i++)
    {
        printf("\nConnection %d\n",i+1);

        printf("From : N%d\n",
               connections[i].from);

        printf("To   : N%d\n",
               connections[i].to);

        printf("Type : %s\n",
               connections[i].type);

        if(connections[i].expected_resistance!=-1)
        {
            printf("Resistance : %d\n",
                connections[i].expected_resistance);
        }

        if(strcmp(connections[i].state,"NONE")!=0)
        {
            printf("State : %s\n",
                connections[i].state);
        }
    }

    printf("\n=======================================\n");
}