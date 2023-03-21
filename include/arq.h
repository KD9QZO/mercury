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

#ifndef ARQ_H_
#define ARQ_H_

#include "timer.h"
#include <unistd.h>
#include "tcp_socket.h"
#include "fifo_buffer.h"
#include "telecom_system.h"
#include <iomanip>

//Message status
#define FAILED -2
#define ACK_TIMED_OUT -1
#define FREE 0
#define ADDED_TO_LIST 1
#define ADDED_TO_BATCH_BUFFER 2
#define PENDING_ACK 3
#define ACKED 4
#define RECEIVED 5

// link status
#define DROPPED -1
#define IDLE 0
#define CONNECTING 1
#define CONNECTED 2
#define DISCONNECTING 3
#define LISTENING 4
#define CONNECTION_RECEIVED 5
#define CONNECTION_ACCEPTED 6

//connection status
#define IDLE 0
#define TRANSMITTING_DATA 1
#define RECEIVING 2
#define RECEIVING_ACKS_DATA 3
#define ACKNOWLEDGING_DATA 4
#define TRANSMITTING_CONTROL 5
#define RECEIVING_ACKS_CONTROL 6
#define ACKNOWLEDGING_CONTROL 7


//Message type
#define NONE 0x00
#define DATA_LONG 0x10
#define DATA_SHORT 0x11
#define ACK_CONTROL 0x20
#define ACK_RANGE 0x21
//#define ACK_MULTI 0x22 //TODO: implement

#define CONTROL 0x30 //multi-commands or especial commands

//Control commands
#define START_CONNECTION 0x31
#define TEST_CONNECTION 0x32
#define CLOSE_CONNECTION 0x33
#define KEEP_ALIVE 0x34
#define FILE_START 0x35
#define FILE_END 0x36
#define PIPE_OPEN 0x37
#define PIPE_CLOSE 0x38
#define TRANSMISSION_DONE 0x39
#define BLOCK_END 0x3A

//Error control
#define MEMORY_ERROR -3
#define MESSAGE_LENGTH_ERROR -2
#define ERROR -1
#define SUCCESSFUL 0

//Node role
#define COMMANDER 0
#define RESPONDER 1

void byte_to_bit(char* data_byte, char* data_bit, int nBytes);
void bit_to_byte(char* data_bit, char* data_byte, int nBits);



struct st_message
{
	int ack_timeout;
	int nResends;
	int length;
	char* data;
	char type;
	char id;
	int status;
	cl_timer ack_timer;
};


class cl_arq_controller
{

public:
	cl_arq_controller();
  ~cl_arq_controller();

  int add_message_control(char code);
  int add_message_tx(char type, int length, char* data);
  int add_message_rx(char type, char id, int length, char* data);



  void set_nResends(int nResends);
  void set_ack_timeout(int ack_timeout);
//  void set_keep_alive_timeout(int keep_alive_timeout);
  void set_link_timeout(int link_timeout);
  void set_nMessages(int nMessages);
  void set_max_data_length(int max_data_length);
  void set_batch_size(int batch_size);
  void set_role(int role);
  void set_call_sign(std::string call_sign);

  int get_nOccupied_messages();
  int get_nFree_messages();
  int get_nTotal_messages();
  int get_nToSend_messages();
  int get_nPending_Ack_messages();
  int get_nReceived_messages();
  int get_nAcked_messages();

  int init();


  void update_status();
  void cleanup();

  void register_ack(int message_id);
  void pad_messages_batch_tx(int size);

  void process_main();

  void process_user_command(std::string command);

  void process_messages();

  void process_messages_commander();
  void process_messages_tx_control();
  void process_messages_tx_data();
  void send(st_message* message);
  void send_batch();
  void process_messages_rx_acks_control();
  void process_messages_rx_acks_data();
  void process_control_commander();

  void process_messages_responder();
  void process_messages_rx_data_control();
  void process_messages_acknowledging_control();
  void process_messages_acknowledging_data();
  void process_control_responder();
  void copy_data_to_buffer();

  void receive();

  void print_stats();

  int message_transmission_time_ms;
  int batch_size;
  int block_ready;
  int max_message_length;
  int max_data_length;

  int connection_status;
  int link_status;
  int role;
  char connection_id;
  char assigned_connection_id;

  int next_connection_status;


  cl_tcp_socket tcp_socket_control;
  cl_tcp_socket tcp_socket_data;


  cl_timer link_timer;
  cl_timer receiving_timer;


  int message_batch_counter_tx;

//  int data_ready_to_send;
//  int data_received;
//  char* message_TxRx_bit_buffer;
  char* message_TxRx_byte_buffer;
  struct st_message messages_rx_buffer;

  struct st_message messages_control;
  struct st_message* messages_batch_tx;

  int ack_timeout;
  int link_timeout;
  std::string distination_call_sign;

  cl_fifo_buffer fifo_buffer_tx;
  cl_fifo_buffer fifo_buffer_rx;

  cl_telecom_system* telecom_system;

private:
  int nMessages;
  struct st_message* messages_tx;
  struct st_message* messages_rx;


  struct st_message* messages_batch_ack;


  std::string my_call_sign;





  int nResends;

  int nSent_messages;
  int nAcked_messages;
  int nReceived_messages;
  int nLost_messages;
  int nNAcked_messages;
  int nReSent_messages;
  int nAcks_sent_messages;
  int nKeep_alive_messages;

  int ack_batch_padding;
};


#endif
