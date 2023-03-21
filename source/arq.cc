/*
 * Mercury: A configurable open-source software-defined modem.
 * Copyright (C) 2023 Fadi Jerji
 * Author: Fadi Jerji
 * Email: fadi.jerji@  <gmail.com, rhizomatica.org, caisresearch.com, ieee.org>
 * ORCID: 0000-0002-2076-5831
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "arq.h"

void byte_to_bit(char* data_byte, char* data_bit, int nBytes)
{
	char mask;
	for(int i=0;i<nBytes;i++)
	{
		mask=0x01;
		for(int j=0;j<8;j++)
		{
			data_bit[i*8+j]=((data_byte[i]&mask)==mask);
			mask=mask<<1;
		}
	}
}

void bit_to_byte(char* data_bit, char* data_byte, int nBits)
{
	char mask;
	for(int i=0;i<nBits/8;i++)
	{
		data_byte[i]=0x00;
		mask=0x01;
		for(int j=0;j<8;j++)
		{
			data_byte[i]|=data_bit[i*8+j]*mask;
			mask=mask<<1;
		}
	}
	if(nBits%8!=0)
	{
		data_byte[nBits/8]=0x00;
		mask=0x01;
		for(int j=0;j<nBits%8;j++)
		{
			data_byte[nBits/8]|=data_bit[nBits-(nBits%8)+j]*mask;
			mask=mask<<1;
		}
	}
}

cl_arq_controller::cl_arq_controller()
{
	connection_status=IDLE;
	next_connection_status=IDLE;
	link_status=IDLE;
	nMessages=0;
	max_message_length=0;
	max_data_length=0;
	messages_tx=NULL;
	messages_rx=NULL;
	message_TxRx_byte_buffer=NULL;
	messages_batch_tx=NULL;
	messages_batch_ack=NULL;
	message_batch_counter_tx=0;
	ack_timeout=1000;
	link_timeout=10000;
	nResends=3;
	nSent_messages=0;
	nAcked_messages=0;
	nReceived_messages=0;
	nLost_messages=0;
	nNAcked_messages=0;
	nReSent_messages=0;
	nAcks_sent_messages=0;
	nKeep_alive_messages=0;
	batch_size=1;
	message_transmission_time_ms=500;
	role=RESPONDER;
	connection_id=0;
	assigned_connection_id=0;
	block_ready=0;
	ack_batch_padding=1;
	my_call_sign="";
	distination_call_sign="";
	telecom_system=NULL;

}



cl_arq_controller::~cl_arq_controller()
{
	for(int i=0;i<nMessages;i++)
	{
		if(messages_tx[i].data!=NULL)
		{
			delete[] messages_tx[i].data;
		}
		if(messages_rx[i].data!=NULL)
		{
			delete[] messages_rx[i].data;
		}
	}
	for(int i=0;i<batch_size;i++)
	{
		if(messages_batch_ack[i].data!=NULL)
		{
			delete[] messages_batch_ack[i].data;
		}
	}
	if(messages_control.data!=NULL)
	{
		delete[] messages_control.data;
	}
	if(messages_rx_buffer.data!=NULL)
	{
		delete[] messages_rx_buffer.data;
	}
	if(messages_tx!=NULL)
	{
		delete[] messages_tx;
	}
	if(messages_rx!=NULL)
	{
		delete[] messages_rx;
	}
	if(messages_batch_tx!=NULL)
	{
		delete[] messages_batch_tx;
	}
	if(messages_batch_ack!=NULL)
	{
		delete[] messages_batch_ack;
	}
	if(message_TxRx_byte_buffer!=NULL)
	{
		delete[] message_TxRx_byte_buffer;
	}
}
int cl_arq_controller::add_message_control(char code)
{
	int success=ERROR;
	if (messages_control.status==FREE)
	{
		messages_control.type=CONTROL;
		if(code==START_CONNECTION)
		{
			messages_control.length=my_call_sign.length()+distination_call_sign.length()+3;

			messages_control.data[0]=code;
			messages_control.data[1]=my_call_sign.length();
			messages_control.data[2]=distination_call_sign.length();

			for(int j=0;j<(int)my_call_sign.length();j++)
			{
				messages_control.data[j+3]=my_call_sign[j];
			}
			for(int j=0;j<(int)distination_call_sign.length();j++)
			{
				messages_control.data[j+my_call_sign.length()+3]=distination_call_sign[j];
			}
			messages_control.id=0;
		}
		else
		{
			messages_control.length=1;
			for(int j=0;j<messages_control.length;j++)
			{
				messages_control.data[j]=code;
			}
			messages_control.id=0;
		}
		messages_control.nResends=this->nResends;
		messages_control.ack_timeout=this->ack_timeout;
		messages_control.status=ADDED_TO_LIST;
		success=SUCCESSFUL;
		this->connection_status=TRANSMITTING_CONTROL;
		print_stats();
	}
	return success;
}

int cl_arq_controller::add_message_tx(char type, int length, char* data)
{
	int success=ERROR;
	if(length<0 && length>=max_data_length)
	{
		success=MESSAGE_LENGTH_ERROR;
	}
	else
	{
		for(int i=0;i<nMessages;i++)
		{
			if (messages_tx[i].status==FREE)
			{
				messages_tx[i].type=type;
				messages_tx[i].length=length;
				for(int j=0;j<messages_tx[i].length;j++)
				{
					messages_tx[i].data[j]=data[j];
				}
				messages_tx[i].id=i;
				messages_tx[i].nResends=this->nResends;
				messages_tx[i].ack_timeout=this->ack_timeout;
				messages_tx[i].status=ADDED_TO_LIST;
				success=SUCCESSFUL;
				print_stats();
				break;
			}
		}
	}
	return success;
}

int cl_arq_controller::add_message_rx(char type, char id, int length, char* data)
{
	int success=ERROR;
	int loc=(int)id;
	if(loc<nMessages)
	{
		if(length<0 && length>=max_data_length)
		{
			success=MESSAGE_LENGTH_ERROR;
		}
		else
		{
			messages_rx[loc].type=type;
			messages_rx[loc].length=length;
			for(int j=0;j<messages_rx[loc].length;j++)
			{
				messages_rx[loc].data[j]=data[j];
			}
			for(int j=messages_rx[loc].length;j<max_data_length;j++)
			{
				messages_rx[loc].data[j]=0;
			}
			messages_rx[loc].status=RECEIVED;
			success=SUCCESSFUL;
			nReceived_messages++;
			print_stats();
		}
	}
	return success;
}

void cl_arq_controller::set_nResends(int nResends)
{
	if(nResends>0)
	{
		this->nResends=nResends;
	}
}


void cl_arq_controller::set_ack_timeout(int ack_timeout)
{
	if(ack_timeout>0)
	{
		this->ack_timeout=ack_timeout;
	}
}


void cl_arq_controller::set_link_timeout(int link_timeout)
{
	if(link_timeout>0)
	{
		this->link_timeout=link_timeout;
	}
}


void cl_arq_controller::set_nMessages(int nMessages)
{
	if(nMessages>0|| nMessages<256)
	{
		this->nMessages=nMessages;
	}
}

void cl_arq_controller::set_max_data_length(int max_data_length)
{
	int header_size=4;
	if(max_data_length>0)
	{
		this->max_data_length=max_data_length;
		this->max_message_length=max_data_length+header_size;
	}
}

void cl_arq_controller::set_batch_size(int batch_size)
{
	if (batch_size>0)
	{
		this->batch_size=batch_size;
	}
}

void cl_arq_controller::set_role(int role)
{
	if(role==COMMANDER)
	{
		this->role=role;
	}
	else
	{
		this->role=RESPONDER;
	}
}

void cl_arq_controller::set_call_sign(std::string call_sign)
{
	this->my_call_sign=call_sign;
}

int cl_arq_controller::get_nOccupied_messages()
{
	int nOccupied_messages=0;
	for(int i=0;i<this->nMessages;i++)
	{
		if(this->messages_tx[i].status!=FREE)
		{
			nOccupied_messages++;
		}
	}
	return nOccupied_messages;
}

int cl_arq_controller::get_nFree_messages()
{
	int nFree_messages=0;
	for(int i=0;i<this->nMessages;i++)
	{
		if(this->messages_tx[i].status==FREE)
		{
			nFree_messages++;
		}
	}
	return nFree_messages;
}

int cl_arq_controller::get_nTotal_messages()
{
	return this->nMessages;
}

int cl_arq_controller::get_nToSend_messages()
{
	int nMessages_to_send=0;
	for(int i=0;i<this->nMessages;i++)
	{
		if(messages_tx[i].status==ADDED_TO_LIST)
		{
			nMessages_to_send++;
		}
		else if (messages_tx[i].status==ACK_TIMED_OUT && messages_tx[i].nResends>0)
		{
			nMessages_to_send++;
		}
	}
	return nMessages_to_send;
}

int cl_arq_controller::get_nPending_Ack_messages()
{
	int nPending_Ack_messages=0;
	for(int i=0;i<this->nMessages;i++)
	{
		if(this->messages_tx[i].status==PENDING_ACK)
		{
			nPending_Ack_messages++;
		}
	}
	return nPending_Ack_messages;
}

int cl_arq_controller::get_nReceived_messages()
{
	int nReceived_messages=0;
	for(int i=0;i<this->nMessages;i++)
	{
		if(this->messages_rx[i].status==RECEIVED)
		{
			nReceived_messages++;
		}
	}
	return nReceived_messages;
}

int cl_arq_controller::get_nAcked_messages()
{
	int nAcked_messages=0;
	for(int i=0;i<this->nMessages;i++)
	{
		if(this->messages_rx[i].status==ACKED)
		{
			nAcked_messages++;
		}
	}
	return nAcked_messages;
}



int cl_arq_controller::init()
{
	int success=SUCCESSFUL;
	this->messages_tx=new st_message[nMessages];

	if(this->messages_tx==NULL)
	{
		success=MEMORY_ERROR;
	}
	else
	{
		for(int i=0;i<this->nMessages;i++)
		{
			this->messages_tx[i].ack_timeout=0;
			this->messages_tx[i].id=0;
			this->messages_tx[i].length=0;
			this->messages_tx[i].nResends=0;
			this->messages_tx[i].status=FREE;
			this->messages_tx[i].type=NONE;
			this->messages_tx[i].data=NULL;

			this->messages_tx[i].data=new char[max_data_length];
			if(this->messages_tx[i].data==NULL)
			{
				success=MEMORY_ERROR;
			}
		}
	}
	
	this->messages_rx=new st_message[nMessages];

	if(this->messages_rx==NULL)
	{
		success=MEMORY_ERROR;
	}
	else
	{
		for(int i=0;i<this->nMessages;i++)
		{
			this->messages_rx[i].ack_timeout=0;
			this->messages_rx[i].id=0;
			this->messages_rx[i].length=0;
			this->messages_rx[i].nResends=0;
			this->messages_rx[i].status=FREE;
			this->messages_rx[i].type=NONE;
			this->messages_rx[i].data=NULL;

			this->messages_rx[i].data=new char[max_data_length];
			if(this->messages_rx[i].data==NULL)
			{
				success=MEMORY_ERROR;
			}
		}
	}

	this->messages_batch_tx=new st_message[batch_size];

	if(this->messages_batch_tx==NULL)
	{
		success=MEMORY_ERROR;
	}
	else
	{
		for(int i=0;i<this->batch_size;i++)
		{
			this->messages_batch_tx[i].ack_timeout=0;
			this->messages_batch_tx[i].id=0;
			this->messages_batch_tx[i].length=0;
			this->messages_batch_tx[i].nResends=0;
			this->messages_batch_tx[i].status=FREE;
			this->messages_batch_tx[i].type=NONE;
			this->messages_batch_tx[i].data=NULL;
		}
	}

	this->messages_batch_ack=new st_message[batch_size];

	if(this->messages_batch_ack==NULL)
	{
		success=MEMORY_ERROR;
	}
	else
	{
		for(int i=0;i<this->batch_size;i++)
		{
			this->messages_batch_ack[i].ack_timeout=0;
			this->messages_batch_ack[i].id=0;
			this->messages_batch_ack[i].length=0;
			this->messages_batch_ack[i].nResends=0;
			this->messages_batch_ack[i].status=FREE;
			this->messages_batch_ack[i].type=NONE;
			this->messages_batch_ack[i].data=NULL;

			this->messages_batch_ack[i].data=new char[max_data_length];
			if(this->messages_batch_ack[i].data==NULL)
			{
				success=MEMORY_ERROR;
			}
		}
	}

	this->messages_control.data=NULL;
	this->messages_control.data=new char[max_data_length];
	if(this->messages_control.data==NULL)
	{
		success=MEMORY_ERROR;
	}

	this->messages_rx_buffer.data=NULL;
	this->messages_rx_buffer.data=new char[max_data_length];
	if(this->messages_rx_buffer.data==NULL)
	{
		success=MEMORY_ERROR;
	}

	this->message_TxRx_byte_buffer=new char[max_message_length];
	if(this->message_TxRx_byte_buffer==NULL)
	{
		success=MEMORY_ERROR;
	}

	return success;
}

void cl_arq_controller::update_status()
{
	for(int i=0;i<nMessages;i++)
	{
		if(messages_tx[i].status==PENDING_ACK && messages_tx[i].ack_timer.get_elapsed_time_ms()>=messages_tx[i].ack_timeout)
		{
			messages_tx[i].status=ACK_TIMED_OUT;
			nNAcked_messages++;
			print_stats();
		}
	}

	if(messages_control.status==PENDING_ACK && messages_control.ack_timer.get_elapsed_time_ms()>=messages_control.ack_timeout)
	{
		messages_control.status=ACK_TIMED_OUT;
		nNAcked_messages++;
		print_stats();
	}

	if(link_timer.get_elapsed_time_ms()>=link_timeout)
	{
		this->link_status=DROPPED;
		print_stats();
		if(this->role==COMMANDER)
		{
			process_control_commander();
		}
		else
		{
			process_control_responder();
		}
		print_stats();
	}

}

void cl_arq_controller::cleanup()
{

	if(messages_control.status==ACKED)
	{
		// SEND ACK TO USER
		this->messages_control.ack_timeout=0;
		this->messages_control.id=0;
		this->messages_control.length=0;
		this->messages_control.nResends=0;
		this->messages_control.status=FREE;
		this->messages_control.type=NONE;
	}
	else if(messages_control.status==FAILED)
	{
		// SEND FAILED TO USER
		this->messages_control.ack_timeout=0;
		this->messages_control.id=0;
		this->messages_control.length=0;
		this->messages_control.nResends=0;
		this->messages_control.status=FREE;
		this->messages_control.type=NONE;
	}


	for(int i=0;i<this->nMessages;i++)
	{
		if(messages_tx[i].status==ACKED)
		{
			// SEND ACK TO USER
			this->messages_tx[i].ack_timeout=0;
			this->messages_tx[i].id=0;
			this->messages_tx[i].length=0;
			this->messages_tx[i].nResends=0;
			this->messages_tx[i].status=FREE;
			this->messages_tx[i].type=NONE;
		}
		else if(messages_tx[i].status==FAILED)
		{
			// SEND FAILED TO USER
			this->messages_tx[i].ack_timeout=0;
			this->messages_tx[i].id=0;
			this->messages_tx[i].length=0;
			this->messages_tx[i].nResends=0;
			this->messages_tx[i].status=FREE;
			this->messages_tx[i].type=NONE;
		}
	}


}

void cl_arq_controller::register_ack(int message_id)
{
	for(int i=0;i<this->nMessages;i++)
	{
		if(messages_tx[i].id==message_id && messages_tx[i].status==PENDING_ACK)
		{
			messages_tx[i].status=ACKED;
			nAcked_messages++;
		}
	}
}

void cl_arq_controller::pad_messages_batch_tx(int size)
{
	if(message_batch_counter_tx!=0 && message_batch_counter_tx<size)
	{
		for(int i=0;i<size-message_batch_counter_tx;i++)
		{
			messages_batch_tx[i+message_batch_counter_tx]=messages_batch_tx[0];
		}
		message_batch_counter_tx=size;
	}
}

void cl_arq_controller::process_main()
{
	std::string command="";
	if(tcp_socket_control.init()==SUCCESS)
	{
		if (tcp_socket_control.get_status()==TCP_STATUS_ACCEPTED)
		{
			if(tcp_socket_control.timer.counting==0)
			{
				tcp_socket_control.timer.start();
			}
			if(tcp_socket_control.receive()>0)
			{
				tcp_socket_control.timer.start();
				command="";
				for(int i=0;i<tcp_socket_control.message->length;i++)
				{
					command+=tcp_socket_control.message->buffer[i];
				}

				process_user_command(command);
				print_stats();
			}
			else
			{
				usleep(1000);
				if(tcp_socket_control.timer.get_elapsed_time_ms()>=tcp_socket_control.timeout_ms)
				{
					tcp_socket_control.check_incomming_connection();
					if (tcp_socket_control.get_status()==TCP_STATUS_ACCEPTED)
					{
						tcp_socket_control.timer.start();
					}
					else
					{
						usleep(10000);
					}
				}
			}

		}
		else
		{
			tcp_socket_control.check_incomming_connection();
			if (tcp_socket_control.get_status()==TCP_STATUS_ACCEPTED)
			{
				tcp_socket_control.timer.start();
			}
			else
			{
				usleep(10000);
			}
		}
	}

	if(tcp_socket_data.init()==SUCCESS)
	{
		if (tcp_socket_data.get_status()==TCP_STATUS_ACCEPTED)
		{
			if(tcp_socket_data.timer.counting==0)
			{
				tcp_socket_data.timer.start();
			}
			if(tcp_socket_data.receive()>0)
			{
				tcp_socket_data.timer.start();
				fifo_buffer_tx.push(tcp_socket_data.message->buffer, tcp_socket_data.message->length);
			}
			else
			{
				usleep(1000);
				if(tcp_socket_data.timer.get_elapsed_time_ms()>=tcp_socket_data.timeout_ms)
				{
					tcp_socket_data.check_incomming_connection();
					if (tcp_socket_data.get_status()==TCP_STATUS_ACCEPTED)
					{
						tcp_socket_data.timer.start();
					}
					else
					{
						usleep(10000);
					}
				}
			}

		}
		else
		{
			tcp_socket_data.check_incomming_connection();
			if (tcp_socket_data.get_status()==TCP_STATUS_ACCEPTED)
			{
				tcp_socket_data.timer.start();
			}
			else
			{
				usleep(10000);
			}
		}
	}

	process_messages();
	usleep(1000);
}

void cl_arq_controller::process_user_command(std::string command)
{

	if(command.substr(0,7)=="MYCALL ")
	{
		this->my_call_sign=command.substr(7);

		tcp_socket_control.message->buffer[0]='O';
		tcp_socket_control.message->buffer[1]='K';
		tcp_socket_control.message->length=2;
	}
	else if(command.substr(0,8)=="CONNECT " && command.substr(8,my_call_sign.length())==my_call_sign)
	{
		this->distination_call_sign=command.substr(9+my_call_sign.length());
		set_role(COMMANDER);
		link_status=CONNECTING;

		tcp_socket_control.message->buffer[0]='O';
		tcp_socket_control.message->buffer[1]='K';
		tcp_socket_control.message->length=2;
	}
	else if(command=="DISCONNECT")
	{
		link_status=DISCONNECTING;
		tcp_socket_control.message->buffer[0]='O';
		tcp_socket_control.message->buffer[1]='K';
		tcp_socket_control.message->length=2;
	}
	else if(command=="LISTEN ON")
	{
		link_status=LISTENING;
		tcp_socket_control.message->buffer[0]='O';
		tcp_socket_control.message->buffer[1]='K';
		tcp_socket_control.message->length=2;
	}
	else if(command=="BW2300")
	{
		tcp_socket_control.message->buffer[0]='O';
		tcp_socket_control.message->buffer[1]='K';
		tcp_socket_control.message->length=2;
	}
	else if(command=="BW2500")
	{
		tcp_socket_control.message->buffer[0]='O';
		tcp_socket_control.message->buffer[1]='K';
		tcp_socket_control.message->length=2;
	}
	else
	{
		tcp_socket_control.message->buffer[0]='N';
		tcp_socket_control.message->buffer[1]='O';
		tcp_socket_control.message->buffer[1]='K';
		tcp_socket_control.message->length=3;
	}


	tcp_socket_control.transmit();
}

void cl_arq_controller::process_messages()
{
	static int  block_under_tx=0;
	int data_read_size;
	this->update_status();
	if(this->role==COMMANDER)
	{
		process_messages_commander();
	}
	else if(this->role==RESPONDER)
	{
		process_messages_responder();
	}

	if(role==COMMANDER && link_status==CONNECTED)
	{
		if( fifo_buffer_tx.get_size()!=fifo_buffer_tx.get_free_size())
		{
			for(int i=0;i<get_nTotal_messages();i++)
			{
				data_read_size=fifo_buffer_tx.pop(message_TxRx_byte_buffer,max_data_length);
				if(data_read_size==0)
				{
					break;
				}
				else if(data_read_size==max_data_length)
				{
					block_under_tx=1;
					add_message_tx(DATA_LONG, data_read_size, message_TxRx_byte_buffer);
				}
				else
				{
					block_under_tx=1;
					add_message_tx(DATA_SHORT, data_read_size, message_TxRx_byte_buffer);
				}
			}
		}
		else if(message_batch_counter_tx!=0 && message_batch_counter_tx<batch_size)
		{
			block_under_tx=1;
			pad_messages_batch_tx(batch_size);
		}
		else if(block_under_tx==1 && message_batch_counter_tx==0 && get_nOccupied_messages()==0 && messages_control.status==FREE)
		{
			add_message_control(BLOCK_END);
			block_under_tx=0;
		}
	}
	else if(role==RESPONDER && link_status==CONNECTED)
	{
		if(tcp_socket_data.init()==SUCCESS)
		{
			if (tcp_socket_data.get_status()==TCP_STATUS_ACCEPTED)
			{
				while(fifo_buffer_rx.get_size()!=fifo_buffer_rx.get_free_size())
				{
					tcp_socket_data.message->length=fifo_buffer_rx.pop(tcp_socket_data.message->buffer,max_data_length);
					tcp_socket_data.transmit();
				}
			}
		}
	}
}

void cl_arq_controller::process_messages_commander()
{
	if(this->link_status==CONNECTING)
	{
		add_message_control(START_CONNECTION);
	}
	else if(this->link_status==CONNECTION_ACCEPTED)
	{
		add_message_control(TEST_CONNECTION);
	}
	else if(this->link_status==DISCONNECTING)
	{
		add_message_control(CLOSE_CONNECTION);
	}


	if(this->connection_status==TRANSMITTING_CONTROL)
	{
		process_messages_tx_control();
	}
	else if(this->connection_status==RECEIVING_ACKS_CONTROL)
	{
		process_messages_rx_acks_control();
	}
	else if(this->connection_status==TRANSMITTING_DATA)
	{
		process_messages_tx_data();
	}
	else if(this->connection_status==RECEIVING_ACKS_DATA)
	{
		process_messages_rx_acks_data();
	}
}

void cl_arq_controller::process_messages_tx_control()
{

	if(messages_control.status==ADDED_TO_LIST&&message_batch_counter_tx<batch_size)
	{
		messages_batch_tx[message_batch_counter_tx]=messages_control;
		message_batch_counter_tx++;
		messages_control.status=ADDED_TO_BATCH_BUFFER;
		nSent_messages++;
	}
	else if(messages_control.status==ACK_TIMED_OUT)
	{
		if(--messages_control.nResends>0&&message_batch_counter_tx<batch_size)
		{
			messages_batch_tx[message_batch_counter_tx]=messages_control;
			message_batch_counter_tx++;
			messages_control.status=ADDED_TO_BATCH_BUFFER;
			nReSent_messages++;
		}
		else
		{
			nLost_messages++;
			print_stats();
			messages_control.status=FAILED;
		}
	}

	if(messages_control.status==ADDED_TO_BATCH_BUFFER)
	{
		pad_messages_batch_tx(1);
		send_batch();
		connection_status=RECEIVING_ACKS_CONTROL;
		receiving_timer.start();
	}
}

void cl_arq_controller::process_messages_tx_data()
{
	for(int i=0;i<this->nMessages;i++)
	{
		if(messages_tx[i].status==ADDED_TO_LIST&&message_batch_counter_tx<batch_size)
		{
			messages_batch_tx[message_batch_counter_tx]=messages_tx[i];
			message_batch_counter_tx++;
			messages_tx[i].status=ADDED_TO_BATCH_BUFFER;
			nSent_messages++;
		}
		else if(messages_tx[i].status==ACK_TIMED_OUT)
		{
			if(--messages_tx[i].nResends>0&&message_batch_counter_tx<batch_size)
			{
				messages_batch_tx[message_batch_counter_tx]=messages_tx[i];
				message_batch_counter_tx++;
				messages_tx[i].status=ADDED_TO_BATCH_BUFFER;
				nReSent_messages++;
			}
			else
			{
				nLost_messages++;
				print_stats();
				messages_tx[i].status=FAILED;
			}

		}

		if(message_batch_counter_tx==batch_size)
		{
			send_batch();
			connection_status=RECEIVING_ACKS_DATA;
			receiving_timer.start();
			break;
		}
	}
}

void cl_arq_controller::send(st_message* message)
{
	int length=0;
	if(message->type==DATA_LONG)
	{
		length=message->length+3;
		message_TxRx_byte_buffer[0]=message->type;
		message_TxRx_byte_buffer[1]=connection_id;
		message_TxRx_byte_buffer[2]=message->id;

		for(int i=0;i<message->length;i++)
		{
			message_TxRx_byte_buffer[i+3]=message->data[i];
		}
	}
	else if (message->type==DATA_SHORT)
	{
		length=message->length+4;
		message_TxRx_byte_buffer[0]=message->type;
		message_TxRx_byte_buffer[1]=connection_id;
		message_TxRx_byte_buffer[2]=message->id;
		message_TxRx_byte_buffer[3]=message->length;

		for(int i=0;i<message->length;i++)
		{
			message_TxRx_byte_buffer[i+4]=message->data[i];
		}
		for(int i=message->length;i<max_data_length;i++)
		{
			message_TxRx_byte_buffer[i+4]=rand()%0xff;
		}
	}
	else if (message->type==ACK_CONTROL)
	{
		length=message->length+2;
		message_TxRx_byte_buffer[0]=message->type;
		message_TxRx_byte_buffer[1]=connection_id;

		for(int i=0;i<message->length;i++)
		{
			message_TxRx_byte_buffer[i+2]=message->data[i];
		}
		for(int i=message->length;i<max_data_length;i++)
		{
			message_TxRx_byte_buffer[i+2]=rand()%0xff;
		}
	}
	else if (message->type==ACK_RANGE)
	{
		length=message->length+3;
		message_TxRx_byte_buffer[0]=message->type;
		message_TxRx_byte_buffer[1]=connection_id;
		message_TxRx_byte_buffer[2]=message->id;

		for(int i=0;i<message->length;i++)
		{
			message_TxRx_byte_buffer[i+3]=message->data[i];
		}
		for(int i=message->length;i<max_data_length;i++)
		{
			message_TxRx_byte_buffer[i+3]=rand()%0xff;
		}
	}
	else if (message->type==CONTROL)
	{
		length=message->length+2;
		message_TxRx_byte_buffer[0]=message->type;
		message_TxRx_byte_buffer[1]=connection_id;

		for(int i=0;i<message->length;i++)
		{
			message_TxRx_byte_buffer[i+2]=message->data[i];
		}
		for(int i=message->length;i<max_data_length;i++)
		{
			message_TxRx_byte_buffer[i+2]=rand()%0xff;
		}
	}
	char message_TxRx_bit_buffer[N_MAX];

	byte_to_bit(message_TxRx_byte_buffer, message_TxRx_bit_buffer, length);

		for(int i=0;i<telecom_system->data_container.nBits-telecom_system->ldpc.P;i++)
		{
			telecom_system->data_container.data[i]=message_TxRx_bit_buffer[i];
		}

		telecom_system->transmit(telecom_system->data_container.data,telecom_system->data_container.passband_data);
		telecom_system->speaker.transfere(telecom_system->data_container.passband_data,telecom_system->data_container.Nofdm*telecom_system->data_container.interpolation_rate*telecom_system->ofdm.Nsymb);

}

void cl_arq_controller::send_batch()
{
	send(&messages_batch_tx[0]);
	for(int i=0;i<message_batch_counter_tx;i++)
	{
		send(&messages_batch_tx[i]);
	}

	for(int i=0;i<message_batch_counter_tx;i++)
	{
		if(messages_batch_tx[i].type==DATA_LONG || messages_batch_tx[i].type==DATA_SHORT)
		{
			messages_tx[(int)messages_batch_tx[i].id].ack_timer.start();
			messages_tx[(int)messages_batch_tx[i].id].status=PENDING_ACK;
		}
		if(messages_batch_tx[i].type==CONTROL)
		{
			messages_control.ack_timer.start();
			messages_control.status=PENDING_ACK;
		}
		print_stats();

		this->messages_batch_tx[i].ack_timeout=0;
		this->messages_batch_tx[i].id=0;
		this->messages_batch_tx[i].length=0;
		this->messages_batch_tx[i].nResends=0;
		this->messages_batch_tx[i].status=FREE;
		this->messages_batch_tx[i].type=NONE;

	}
	message_batch_counter_tx=0;

}

void cl_arq_controller::receive()
{

	int received_message[N_MAX];
	char received_message_char[N_MAX];
	st_receive_stats received_message_stats;
	if(telecom_system->data_container.data_ready==1)
	{
		telecom_system->data_container.data_ready=0;

		if(telecom_system->data_container.frames_to_read==0)
		{
			received_message_stats=telecom_system->receive((const double*)telecom_system->data_container.passband_delayed_data,received_message);
			shift_left(telecom_system->data_container.passband_delayed_data, telecom_system->data_container.Nofdm*telecom_system->data_container.interpolation_rate*telecom_system->data_container.buffer_Nsymb, telecom_system->data_container.Nofdm*telecom_system->data_container.interpolation_rate);


			if (received_message_stats.message_decoded==YES)
			{
				telecom_system->data_container.frames_to_read=telecom_system->data_container.Nsymb;
				for(int i=0;i<N_MAX;i++)
				{
					received_message_char[i]=(char)received_message[i];
				}
				bit_to_byte(received_message_char, message_TxRx_byte_buffer, telecom_system->data_container.nBits);

				if(message_TxRx_byte_buffer[1]==this->connection_id)
				{
					messages_rx_buffer.status=RECEIVED;
					messages_rx_buffer.type=message_TxRx_byte_buffer[0];
					if(messages_rx_buffer.type==ACK_CONTROL || messages_rx_buffer.type==ACK_RANGE || messages_rx_buffer.type==CONTROL)
					{
						for(int j=0;j<max_data_length;j++)
						{
							messages_rx_buffer.data[j]=message_TxRx_byte_buffer[j+2];
						}
					}
					else if(messages_rx_buffer.type==DATA_LONG)
					{
						messages_rx_buffer.id=message_TxRx_byte_buffer[2];
						messages_rx_buffer.length=max_data_length;
						for(int j=0;j<max_data_length;j++)
						{
							messages_rx_buffer.data[j]=message_TxRx_byte_buffer[j+3];
						}
					}
					else if(messages_rx_buffer.type==DATA_SHORT)
					{
						messages_rx_buffer.id=message_TxRx_byte_buffer[2];
						messages_rx_buffer.length=message_TxRx_byte_buffer[3];
						for(int j=0;j<messages_rx_buffer.length;j++)
						{
							messages_rx_buffer.data[j]=message_TxRx_byte_buffer[j+4];
						}
					}
				}
			}
		}
	}

}

void cl_arq_controller::process_messages_rx_acks_control()
{
	if (receiving_timer.get_elapsed_time_ms()<(batch_size+2)*message_transmission_time_ms)
	{
		this->receive();
		if(messages_rx_buffer.status==RECEIVED && messages_rx_buffer.type==ACK_CONTROL)
		{
			if(messages_rx_buffer.data[0]==messages_control.data[0])
			{
				for(int j=0;j<max_data_length;j++)
				{
					messages_control.data[j]=messages_rx_buffer.data[j];
				}
				link_timer.start();
				messages_control.status=ACKED;
				process_control_commander();
			}
			messages_rx_buffer.status=FREE;
			print_stats();
		}
		this->cleanup();
	}
	else
	{
		connection_status=TRANSMITTING_CONTROL;
		print_stats();
	}
}

void cl_arq_controller::process_messages_rx_acks_data()
{
	if (receiving_timer.get_elapsed_time_ms()<(batch_size+2)*message_transmission_time_ms)
	{
		this->receive();
		if(messages_rx_buffer.status==RECEIVED)
		{
			if(messages_rx_buffer.type==ACK_RANGE)
			{
				link_timer.start();
				char start=messages_rx_buffer.data[0];
				char end=messages_rx_buffer.data[1];
				for(char i=start;i<=end;i++)
				{
					register_ack(i);
				}

				print_stats();
			}
			messages_rx_buffer.status=FREE;
		}
		this->cleanup();

		if(get_nPending_Ack_messages()==0 && receiving_timer.get_elapsed_time_ms()>(ack_batch_padding+1)*message_transmission_time_ms)
		{
			connection_status=TRANSMITTING_DATA;
			print_stats();
		}
	}
	else
	{
		connection_status=TRANSMITTING_DATA;
		print_stats();
	}
}

void cl_arq_controller::process_control_commander()
{
	if(this->link_status==DROPPED)
	{
		set_role(RESPONDER);
		connection_id=0;
		assigned_connection_id=0;
		link_timer.stop();
		link_timer.reset();
		// SEND TO USER
	}

	if(this->connection_status==RECEIVING_ACKS_CONTROL)
	{
		if(this->link_status==CONNECTING && messages_control.data[0]==START_CONNECTION)
		{
			this->link_status=CONNECTION_ACCEPTED;
			this->connection_id=messages_control.data[1];
		}
		else if(this->link_status==CONNECTION_ACCEPTED && messages_control.data[0]==TEST_CONNECTION)
		{
			this->link_status=CONNECTED;
		}
		else if(this->link_status==CONNECTED)
		{
			if (messages_control.data[0]==FILE_END)
			{
				std::cout<<"end of file"<<std::endl;
			}
			else if (messages_control.data[0]==BLOCK_END)
			{
				std::cout<<"end of block"<<std::endl;
			}
		}
		else if(this->link_status==DISCONNECTING && messages_control.data[0]==CLOSE_CONNECTION)
		{
			set_role(RESPONDER);
			this->link_status=IDLE;
		}

		this->connection_status=TRANSMITTING_DATA;
	}
}


void cl_arq_controller::process_messages_responder()
{

	if(this->connection_status==ACKNOWLEDGING_CONTROL)
	{
		process_messages_acknowledging_control();
	}
	else if(this->connection_status==ACKNOWLEDGING_DATA)
	{
		process_messages_acknowledging_data();
	}
	else
	{
		process_messages_rx_data_control();
	}

}


void cl_arq_controller::process_messages_rx_data_control()
{
	if (receiving_timer.get_elapsed_time_ms()<(batch_size+2)*message_transmission_time_ms)
	{
		this->receive();
		if(messages_rx_buffer.status==RECEIVED)
		{
			if(messages_rx_buffer.type==CONTROL)
			{
				messages_control.type=messages_rx_buffer.type;
				messages_control.id=0;
				messages_control.status=messages_rx_buffer.status;
				messages_control.length=1;
				for(int j=0;j<max_data_length;j++)
				{
					messages_control.data[j]=message_TxRx_byte_buffer[j+2];
				}

				process_control_responder();
				connection_status=ACKNOWLEDGING_CONTROL;
				print_stats();
				messages_rx_buffer.status=FREE;
				return;
			}
			else if(messages_rx_buffer.type==DATA_LONG || messages_rx_buffer.type==DATA_SHORT)
			{
				add_message_rx(messages_rx_buffer.type, messages_rx_buffer.id, messages_rx_buffer.length, messages_rx_buffer.data);
				if(receiving_timer.get_counter_status()==0)
				{
					receiving_timer.start();
				}
			}
			messages_rx_buffer.status=FREE;
		}

		if(get_nReceived_messages()>=batch_size)
		{
			connection_status=ACKNOWLEDGING_DATA;
			print_stats();
		}
	}
	else if ( get_nReceived_messages()!=0)
	{
		connection_status=ACKNOWLEDGING_DATA;
		print_stats();
	}
	else
	{
		receiving_timer.stop();
		receiving_timer.reset();
	}
}

void cl_arq_controller::process_messages_acknowledging_control()
{
	message_batch_counter_tx=0;
	if(messages_control.status==RECEIVED)
	{
		messages_control.type=ACK_CONTROL;
		messages_control.status=ACKED;
		messages_batch_tx[message_batch_counter_tx]=messages_control;
		message_batch_counter_tx++;
		nAcks_sent_messages++;
		//pad_messages_batch_tx(ack_batch_padding);

		send_batch();
		connection_status=next_connection_status;
		connection_id=assigned_connection_id;
		print_stats();
	}
}


void cl_arq_controller::process_messages_acknowledging_data()
{
	int nAcks_sent=0;
	int nAck_messages=0;
	receiving_timer.stop();
	receiving_timer.reset();

	int half_batch_size=0;
	if(batch_size%2==0)
	{
		half_batch_size=batch_size/2;
	}
	else
	{
		half_batch_size=batch_size/2 + 1;
	}

	message_batch_counter_tx=0;
	for(int j=0;j<half_batch_size;j++)
	{
		int start=-1;
		int end=-1;
		for(int i=0;i<this->nMessages;i++)
		{
			if(messages_rx[i].status==RECEIVED)
			{
				start=i;
				end=i;
				break;
			}
		}
		for(int i=start+1;i<this->nMessages;i++)
		{
			if(messages_rx[i].status==RECEIVED)
			{
				end=i;
			}
			else
			{
				break;
			}
		}

		if(start!=-1)
		{
			messages_batch_ack[message_batch_counter_tx].type=ACK_RANGE;
			messages_batch_ack[message_batch_counter_tx].id=(char)start;
			messages_batch_ack[message_batch_counter_tx].length=1;
			messages_batch_ack[message_batch_counter_tx].data=new char;
			messages_batch_ack[message_batch_counter_tx].data[0]=end;
			nAcks_sent=end-start+1;

			for(int i=start;i<=end;i++)
			{
				messages_rx[i].status=ACKED;
			}

			messages_batch_tx[message_batch_counter_tx]=messages_batch_ack[message_batch_counter_tx];
			message_batch_counter_tx++;
			nAck_messages++;
			nAcks_sent_messages+=nAcks_sent;
		}

		if(nAcks_sent>=half_batch_size|| get_nReceived_messages()==0)
		{
			link_timer.start();
			pad_messages_batch_tx(ack_batch_padding);
			send_batch();
			connection_status=RECEIVING;
			print_stats();
			break;
		}
	}
}

void cl_arq_controller::process_control_responder()
{
	char code=messages_control.data[0];
	if(link_status==LISTENING && code==START_CONNECTION)
	{
		int call_for_me=1;
		if(messages_control.data[2]!=(char)my_call_sign.length())
		{
			call_for_me=0;
		}
		else
		{
			for(int i=0;i<messages_control.data[2];i++)
			{
				if(my_call_sign[i]!=messages_control.data[messages_control.data[1]+3+i])
				{
					call_for_me=0;
				}
			}
		}
		if(call_for_me==1)
		{
			link_timer.start();
			distination_call_sign="";
			for(int i=0;i<messages_control.data[1];i++)
			{
				distination_call_sign+=messages_control.data[3+i];
			}

			link_status=CONNECTION_RECEIVED;
			messages_control.data[1]=rand()%0xff;
			messages_control.length=2;
			assigned_connection_id=messages_control.data[1];
			next_connection_status=RECEIVING;
		}
		else
		{
			messages_control.status=FREE;
		}
	}
	else if(link_status==CONNECTION_RECEIVED && code==TEST_CONNECTION)
	{
		link_timer.start();
		link_status=CONNECTED;
		next_connection_status=RECEIVING;
		link_timer.start();
	}
	else if(link_status==CONNECTED && code==TEST_CONNECTION)
	{
		next_connection_status=RECEIVING;
		link_timer.start();
	}
	else if(link_status==CONNECTED && code==CLOSE_CONNECTION)
	{
		//SEND TO USER
		assigned_connection_id=0;
		link_status=LISTENING;
		next_connection_status=RECEIVING;
		link_timer.stop();
		link_timer.reset();
	}
	else if (link_status==DROPPED)
	{
		connection_id=0;
		assigned_connection_id=0;
		link_status=LISTENING;
		connection_status=RECEIVING;
		link_timer.stop();
		link_timer.reset();
	}

	if(code==BLOCK_END)
	{
		std::cout<<"end of block"<<std::endl;
		copy_data_to_buffer();
		next_connection_status=RECEIVING;
	}
	else if(code==FILE_END)
	{
		std::cout<<"end of file"<<std::endl;
		copy_data_to_buffer();
		next_connection_status=RECEIVING;
	}
}


void cl_arq_controller::copy_data_to_buffer() //TODO add buffer overflow
{
	for(int i=0;i<this->nMessages;i++)
	{
		if(messages_rx[i].status==ACKED)
		{
			fifo_buffer_rx.push(messages_rx[i].data,messages_rx[i].length);
			messages_rx[i].status=FREE;
		}
	}
	block_ready=1;
}

void cl_arq_controller::print_stats()
{
	printf ("\e[2J");// clean screen
	printf ("\e[H"); // go to upper left corner
	if(this->role==COMMANDER)
	{
		std::cout<<"Role:COM";
	}
	else if (this->role==RESPONDER)
	{
		std::cout<<"Role:Res";
	}

	if(this->link_status==DROPPED)
	{
		std::cout<<", link_status:Dropd";
	}
	else if(this->link_status==IDLE)
	{
		std::cout<<", link_status:Idle";
	}
	else if (this->link_status==CONNECTING)
	{
		std::cout<<", link_status:Conecting";
	}
	else if (this->link_status==CONNECTED)
	{
		std::cout<<", link_status:conntd";
	}
	else if (this->link_status==DISCONNECTING)
	{
		std::cout<<", link_status:Disconing";
	}
	else if (this->link_status==LISTENING)
	{
		std::cout<<", link_status:List";
	}
	else if (this->link_status==CONNECTION_RECEIVED)
	{
		std::cout<<", link_status:Conn_Rx";
	}
	else if (this->link_status==CONNECTION_ACCEPTED)
	{
		std::cout<<", link_status:Conn_Acptd";
	}

	if (this->connection_status==TRANSMITTING_DATA)
	{
		std::cout<<", connection_status:TX_D";
	}
	else if (this->connection_status==RECEIVING)
	{
		std::cout<<", connection_status:RX";
	}
	else if (this->connection_status==RECEIVING_ACKS_DATA)
	{
		std::cout<<", connection_status:RXAck_D";
	}
	if(this->connection_status==ACKNOWLEDGING_DATA)
	{
		std::cout<<", connection_status:Ack_D";
	}
	else if (this->connection_status==TRANSMITTING_CONTROL)
	{
		std::cout<<", connection_status:TX_C";
	}
	else if (this->connection_status==RECEIVING_ACKS_CONTROL)
	{
		std::cout<<", connection_status:RXAck_C";
	}
	else if (this->connection_status==ACKNOWLEDGING_CONTROL)
	{
		std::cout<<", connection_status:Ack_C";
	}

	std::cout<<", Sent:"<<this->nSent_messages;
	//	std::cout<<", ReSent:"<<this->nReSent_messages;
	std::cout<<", Acked:"<<this->nAcked_messages;
	//	std::cout<<", nAcked:"<<this->nNAcked_messages;
	std::cout<<", Lost:"<<this->nLost_messages;
	std::cout<<", ToSend:"<<this->get_nToSend_messages();
	std::cout<<", Rec:"<<this->nReceived_messages;
	std::cout<<", nAck_sent:"<<this->nAcks_sent_messages;
	std::cout<<", nKeepAlive_sent:"<<this->nKeep_alive_messages;
	std::cout<<std::endl;

	std::cout << std::fixed;
	std::cout << std::setprecision(1);
//	std::cout<<"decoded in "<<received_message_stats.iterations_done<<" data=";
//	for(int i=0;i<10;i++)
//	{
//		std::cout<<received_message[i]<<",";
//	}
//	std::cout<<"delay="<<received_message_stats.delay;
//	std::cout<<" 1st symb loc="<<received_message_stats.first_symbol_location;
	std::cout<<" freq_offset="<<telecom_system->receive_stats.freq_offset;
	std::cout<<" SNR="<<telecom_system->receive_stats.SNR<<" dB";
	std::cout<<" Signal Strength="<<telecom_system->receive_stats.signal_stregth_dbm<<" dBm ";

	std::cout<<std::endl;
}



