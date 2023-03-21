/*
 * Mercury: A configurable open-source software-defined modem.
 * Copyright (C) 2022 Fadi Jerji
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

#include <iostream>
#include <complex>
#include "telecom_system.h"
#include <fstream>
#include <math.h>
#include <iostream>
#include <complex>
#include <iomanip>
#include "arq.h"



int main(int argc, char *argv[])
{
	srand(time(0));

	cl_telecom_system telecom_system;

	telecom_system.operation_mode=ARQ_MODE;

	telecom_system.M=MOD_64QAM;
	telecom_system.psk.set_predefined_constellation(telecom_system.M);
	telecom_system.awgn_channel.set_seed(rand());
	telecom_system.test_tx_AWGN_EsN0_calibration=1;
	telecom_system.test_tx_AWGN_EsN0=50;

	telecom_system.ofdm.Nc=AUTO_SELLECT;
	telecom_system.ofdm.Nfft=512;
	telecom_system.ofdm.gi=1.0/16.0;
	telecom_system.ofdm.Nsymb=AUTO_SELLECT;
	telecom_system.ofdm.pilot_configurator.Dx=AUTO_SELLECT;
	telecom_system.ofdm.pilot_configurator.Dy=AUTO_SELLECT;
	telecom_system.ofdm.pilot_configurator.first_row=DATA;
	telecom_system.ofdm.pilot_configurator.last_row=DATA;
	telecom_system.ofdm.pilot_configurator.first_col=DATA;
	telecom_system.ofdm.pilot_configurator.second_col=DATA;
	telecom_system.ofdm.pilot_configurator.last_col=AUTO_SELLECT;
	telecom_system.ofdm.pilot_configurator.pilot_boost=1.33;
	telecom_system.ofdm.pilot_configurator.first_row_zeros=YES;
	telecom_system.ofdm.print_time_sync_status=NO;
	telecom_system.ofdm.freq_offset_ignore_limit=0.1;

	telecom_system.ldpc.standard=HERMES;
	telecom_system.ldpc.framesize=HERMES_NORMAL;
	telecom_system.ldpc.rate=14/16.0;
	telecom_system.ldpc.decoding_algorithm=GBF;
	telecom_system.ldpc.GBF_eta=0.5;
	telecom_system.ldpc.nIteration_max=100;
	telecom_system.ldpc.print_nIteration=NO;

	telecom_system.bandwidth=2300;
	telecom_system.time_sync_trials_max=1;
	telecom_system.lock_time_sync=YES;
	telecom_system.frequency_interpolation_rate=1;
	telecom_system.carrier_frequency=1450;
	telecom_system.output_power_Watt=1.6;
	telecom_system.filter_window=HANNING;
	telecom_system.filter_transition_bandwidth=1000;
	telecom_system.filter_cut_frequency=6000;

	telecom_system.init();

	telecom_system.bit_interleaver_block_size=telecom_system.data_container.nBits/10;
	telecom_system.ofdm.time_sync_Nsymb=telecom_system.ofdm.Nsymb;

	telecom_system.plot.folder="/mnt/ramDisk/";
	telecom_system.plot.plot_active=NO;

	telecom_system.microphone.dev_name="plughw:0,0";
	telecom_system.speaker.dev_name="plughw:0,0";


	telecom_system.data_container.sound_device_ptr=(void*)&telecom_system.microphone;
	telecom_system.microphone.type=CAPTURE;
	telecom_system.microphone.baudrate=telecom_system.sampling_frequency;
	telecom_system.microphone.nbuffer_Samples=2 * telecom_system.ofdm.Nfft*(1+telecom_system.ofdm.gi)*telecom_system.frequency_interpolation_rate*telecom_system.ofdm.Nsymb;
	telecom_system.microphone.channels=LEFT;
	telecom_system.microphone.frames_per_period=telecom_system.ofdm.Nfft*(1+telecom_system.ofdm.gi)*telecom_system.frequency_interpolation_rate;
	telecom_system.microphone.data_container_ptr=&telecom_system.data_container;
	telecom_system.microphone.init();

	telecom_system.speaker.type=PLAY;
	telecom_system.speaker.baudrate=telecom_system.sampling_frequency;
	telecom_system.speaker.nbuffer_Samples=2 * telecom_system.ofdm.Nfft*(1+telecom_system.ofdm.gi)*telecom_system.frequency_interpolation_rate*telecom_system.ofdm.Nsymb;
	telecom_system.speaker.frames_per_period=telecom_system.ofdm.Nfft*(1+telecom_system.ofdm.gi)*telecom_system.frequency_interpolation_rate;
	telecom_system.speaker.channels=MONO;
	telecom_system.speaker.frames_to_leave_transmit_fct=800;
	telecom_system.speaker.init();


	cl_arq_controller ARQ;

	ARQ.telecom_system=&telecom_system;
	int batch_size=5;
	int nMessages=batch_size*4;
	int nBytes_per_message=(telecom_system.data_container.nBits-telecom_system.ldpc.P)/8;

	ARQ.fifo_buffer_tx.set_size(nBytes_per_message*nMessages*10);
	ARQ.fifo_buffer_rx.set_size(nBytes_per_message*nMessages*10);

	ARQ.set_ack_timeout(10000);
	ARQ.set_link_timeout(100000);
	ARQ.set_max_data_length(nBytes_per_message);//MAX 255 bytes
	ARQ.set_nMessages(nMessages); //MAX 255
	ARQ.set_nResends(10);
	ARQ.set_batch_size(batch_size);
	ARQ.init();


	ARQ.tcp_socket_control.port=6002;
	ARQ.tcp_socket_control.timeout_ms=100;

	ARQ.tcp_socket_data.port=6003;
	ARQ.tcp_socket_data.timeout_ms=100;

	ARQ.print_stats();


	while(1)
	{
		ARQ.process_main();
	}


}

