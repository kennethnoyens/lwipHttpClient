/*
	HTTP CLIENT FOR RAW LWIP
	(c) 2008-2009 Noyens Kenneth
	PUBLIC VERSION V0.2 16/05/2009

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU LESSER GENERAL PUBLIC LICENSE Version 2.1 as published by
	the Free Software Foundation.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU Lesser General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the
	Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
	
*/
#include <stdlib.h>
#include <string.h>
#include "httpclient.h"

// Close a PCB(connection)
void hc_clearpcb(struct tcp_pcb *pcb)
{
	if(pcb != NULL)
	{
		// Close the TCP connection
    		tcp_close(pcb);
	}
}

// Function that lwip calls for handling recv'd data
err_t hc_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
	struct hc_state *state = arg;
	char * page = NULL;
	struct pbuf * temp_p;
	hc_errormsg errormsg = GEN_ERROR;
	int i;

    if((err == ERR_OK) && (p != NULL))
    {
		tcp_recved(pcb, p->tot_len);

		// Add payload (p) to state
		temp_p = p;
		while(temp_p != NULL)
		{
			state->RecvData = realloc(state->RecvData, temp_p->len + state->Len + 1);

			// CHECK 'OUT OF MEM'
			if(state->RecvData == NULL)
			{
				// OUT OF MEMORY
				(*state->ReturnPage)(state->Num, OUT_MEM, NULL, 0);	
				return(ERR_OK);
			}

			strncpy(state->RecvData + state->Len, temp_p->payload, temp_p->len);
			state->RecvData[temp_p->len + state->Len] = '\0';			
			state->Len += temp_p->len;

			temp_p = temp_p->next;
		}
		
		// Removing payloads
		
		while(p != NULL)
		{
			temp_p = p->next;
			pbuf_free(p);
			p = temp_p;
		}
		
    }

    // NULL packet == CONNECTION IS CLOSED(by remote host)
    else if((err == ERR_OK) && (p == NULL))
    {	
		// Simple code for checking 200 OK
		for(i=0; i < state->Len; i++)
		{
			if(errormsg == GEN_ERROR)
			{
				// Check for 200 OK 
				if((*(state->RecvData+i) == '2') && (*(state->RecvData+ ++i) == '0') && (*(state->RecvData+ ++i) == '0')) errormsg = OK;
				if(*(state->RecvData+i) == '\n') errormsg = NOT_FOUND;
			}
			else
			{
				// Remove headers
				if((*(state->RecvData+i) == '\r') && (*(state->RecvData+ ++i) == '\n') && (*(state->RecvData+ ++i) == '\r') && (*(state->RecvData + ++i) == '\n'))
				{
					i++;
					page = malloc(strlen(state->RecvData+i));
					strcpy(page, state->RecvData+i);
					break;
				}
			}
		}

		if(errormsg == OK)
		{
			// Put recv data to ---> p->ReturnPage
			(*state->ReturnPage)(state->Num, OK, page, state->Len);
		}
		else
		{
			// 200 OK not found Return NOT_FOUND (WARNING: NOT_FOUND COULD ALSO BE 5xx SERVER ERROR, ...) 
			(*state->ReturnPage)(state->Num, errormsg, NULL, 0);
		}

        // Clear the PCB
        hc_clearpcb(pcb);

		// free the memory containing state
		free(state->RecvData);
		free(state);
    }

    return(ERR_OK);
}

// Function that lwip calls when there is an error
static void hc_error(void *arg, err_t err)
{
    struct hc_state *state = arg;
    // pcb already deallocated

    // Call return function
    // TO-DO: Check err_t err for out_mem, ...
    (*state->ReturnPage)(state->Num, GEN_ERROR, NULL, 0);

    free(state->RecvData);
    free(state->PostVars);
    free(state->Page);
    free(state);
}

// Function that lwip calls when the connection is idle
// Here we can kill connections that have stayed idle for too long
static err_t hc_poll(void *arg, struct tcp_pcb *pcb)
{
    struct hc_state *state = arg;

    state->ConnectionTimeout++;
    if(state->ConnectionTimeout > 20)
    {
        // Close the connection
        tcp_abort(pcb);

		// Give err msg to callback function
		// Call return function
		(*state->ReturnPage)(state->Num, TIMEOUT, NULL, 0);
    }

    return(ERR_OK);
}

// lwip calls this function when the remote host has successfully received data (ack)
static err_t hc_sent(void *arg, struct tcp_pcb *pcb, u16_t len)
{
    struct hc_state *state = arg;

    // Reset connection timeout
    state->ConnectionTimeout = 0;

    return(ERR_OK);
}

// lwip calls this function when the connection is established
static err_t hc_connected(void *arg, struct tcp_pcb *pcb, err_t err)
{
    struct hc_state *state = arg;
    char  * headers;

    // error?
    if(err != ERR_OK)
    {
        hc_clearpcb(pcb);

        // Call return function
        (*state->ReturnPage)(state->Num, GEN_ERROR, NULL, 0);

        // Free wc state
        free(state->RecvData);
        free(state);

        return(ERR_OK);
    }

    // Define Headers
    if(state->PostVars == NULL)
    {	
        // GET headers (without page)(+ \0) = 19
        headers = malloc(19 + strlen(state->Page));
        usprintf(headers,"GET /%s HTTP/1.0\r\n\r\n", state->Page);
    }
    else
    {
        // POST headers (without PostVars or Page)(+ \0) = 91
        // Content-length: %d <== 						   ??? (max 10)
        headers = malloc(91 + strlen(state->PostVars) + strlen(state->Page) + 10);
        usprintf(headers, "POST /%s HTTP/1.0\r\nContent-type: application/x-www-form-urlencoded\r\nContent-length: %d\r\n\r\n%s\r\n\r\n", state->Page, strlen(state->PostVars), state->PostVars);
    }

    // Check if we are nut running out of memory
    if(headers == NULL)
    {
        hc_clearpcb(pcb);

        // Call return function
        (*state->ReturnPage)(state->Num, OUT_MEM, NULL, 0);

        // Free wc state
        free(state->RecvData);
        free(state);

        return(ERR_OK);
    }

    // Setup the TCP receive function
    tcp_recv(pcb, hc_recv);

    // Setup the TCP error function
    tcp_err(pcb, hc_error);

    // Setup the TCP polling function/interval	 //TCP_POLL IS NOT CORRECT DEFINED @ DOC!!!
    tcp_poll(pcb, hc_poll, 10);					 	

    // Setup the TCP sent callback function
    tcp_sent(pcb, hc_sent);

    // Send data
    tcp_write(pcb, headers, strlen(headers), 1);
    tcp_output(pcb);

    // remove headers
    free(headers);
    free(state->PostVars);			// postvars are send, so we don't need them anymore
    free(state->Page);		    	        // page is requested, so we don't need it anymore

    return(ERR_OK);
}


// Public function for request a webpage (REMOTEIP, ...
int hc_open(struct ip_addr remoteIP, char *Page, char *PostVars, void (* returnpage)(u8_t, hc_errormsg, char *, u16_t))
{
	struct tcp_pcb *pcb = NULL;
	struct hc_state *state;
	static u8_t num = 0;
	// local port
	u16_t port= 4545; 	

	// Get a place for a new webclient state in the memory
	state = malloc(sizeof(struct hc_state));

	// Create a new PCB (PROTOCOL CONTROL BLOCK)
	pcb = tcp_new();
	if(pcb == NULL || state == NULL)
	{
		UARTprintf("hc_open: Not enough memory for pcb or state\n");	
		//Not enough memory
		return 0;
	}

	// Define webclient state vars
	num++;
	state->Num = num;
	state->RecvData = NULL;
	state->ConnectionTimeout = 0;
	state->Len = 0;
	state->ReturnPage = returnpage;

	// Make place for PostVars & Page
	if(PostVars != NULL) state->PostVars = malloc(strlen(PostVars) +1);
	state->Page = malloc(strlen(Page) +1);

	// Check for "out of memory"
	if(state->Page == NULL || (state->PostVars == NULL && PostVars != NULL))
	{
		free(state->Page);
		free(state->PostVars);
		free(state);
		tcp_close(pcb);
		return 0;
	}
	// Place allocated copy data 
	strcpy(state->Page, Page);
	if(PostVars != NULL) strcpy(state->PostVars, PostVars);

	// Bind to local IP & local port
	while(tcp_bind(pcb, IP_ADDR_ANY, port) != ERR_OK)
	{
		// Local port in use, use port+1
		port++;
	}

	// Use conn -> argument(s)
	tcp_arg(pcb, state);

	// Open connect (SEND SYN) 
	tcp_connect(pcb, &remoteIP, 80, hc_connected);
	
	return num;
}
