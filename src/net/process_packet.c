#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>

#include "packet_types.h"
#include "unpack_bundles.h"

#include "main.h"
#include "xl3_utils.h"
#include "xl3_rw.h"
#include "xl3_cmds.h"
#include "mtc_utils.h"
#include "mtc_cmds.h"
#include "mtc_init.h"
#include "crate_init.h"
#include "run_pedestals.h"
#include "net_utils.h"
#include "net.h"
#include "fec_test.h"
#include "mem_test.h"
#include "vmon.h"
#include "board_id.h"
#include "ped_run.h"
#include "zdisc.h"
#include "crate_cbal.h"
#include "cgt_test.h"
#include "cald_test.h"
#include "cmos_m_gtvalid.h"
#include "ttot.h"
#include "fifo_test.h"
#include "disc_check.h"
#include "mb_stability_test.h"
#include "chinj_scan.h"
#include "final_test.h"
#include "trigger_scan.h"
#include "see_refl.h"
#include "find_noise.h"
#include "ecal.h"
#include "ecal_to_fec.h"
#include "process_packet.h"

int read_xl3_packet(int fd)
{
  int i,crate = -1;
  for (i=0;i<MAX_XL3_CON;i++){
    if (fd == rw_xl3_fd[i])
      crate = i;
  }
  if (crate == -1){
    printsend("Incorrect xl3 fd? No matching crate\n");
    return -1;
  }
  if ((xl3_lock[crate] == 1) && (reading_from_tut == 0)){
    // this socket is locked and we arent supposed to be reading it
    return 0;
  }
  memset(buffer,'\0',MAX_PACKET_SIZE);
  int numbytes = recv(fd, buffer, MAX_PACKET_SIZE, 0);
  // check if theres any errors or an EOF packet
  if (numbytes < 0){
    printsend("Error receiving data from XL3 #%d (got %d). Error code %d\n",crate,numbytes,errno);
    if (errno == 54){
      printsend("XL3 probably restarted, closing the connection on socket %d and waiting to reconnect.\n",fd);
    }
    pthread_mutex_lock(&main_fdset_lock);
    FD_CLR(fd,&xl3_fdset);
    FD_CLR(fd,&main_fdset);
    close(fd);
    pthread_mutex_unlock(&main_fdset_lock);
    rw_xl3_fd[crate] = -1;
    return -1;
  }else if (numbytes == 0){
    printsend("Got a zero byte packet, Closing XL3 #%d on socket %d\n",crate,fd);
    pthread_mutex_lock(&main_fdset_lock);
    FD_CLR(fd,&xl3_fdset);
    FD_CLR(fd,&main_fdset);
    close(fd);
    pthread_mutex_unlock(&main_fdset_lock);
    rw_xl3_fd[crate] = -1;
    return -1;
  }
  // otherwise, process the packet
  process_xl3_packet(buffer,crate);  
  return 0;
}

int read_control_command(int fd)
{
  if (cont_lock == 1){
    // this socket is locked and we aren't supposed to be reading it
    return 0;
  }
//  memset(buffer,'\0',MAX_PACKET_SIZE);
//  int numbytes = recv(fd, buffer, MAX_PACKET_SIZE, 0);
    memset(buffer,'\0',3000);
    int numbytes = recv(fd, buffer, 3000, 0);
  // check if theres any errors or an EOF packet
  if (numbytes < 0){
    printsend("Error receiving command from controller\n");
    pthread_mutex_lock(&main_fdset_lock);
    FD_CLR(fd,&cont_fdset);
    FD_CLR(fd,&main_fdset);
    close(fd);
    pthread_mutex_unlock(&main_fdset_lock);
    rw_cont_fd = -1;
    return -1;
  }else if (numbytes == 0){
    printsend("Closing controller connection.\n");
    pthread_mutex_lock(&main_fdset_lock);
    FD_CLR(fd,&cont_fdset);
    FD_CLR(fd,&main_fdset);
    close(fd);
    pthread_mutex_unlock(&main_fdset_lock);
    rw_cont_fd = -1;
    return -1;
  }
  int error = 0;
  // if running a final test, pass the command there
  if (running_final_test || running_ecal){
    pthread_mutex_lock(&final_test_cmd_lock);
    sprintf(final_test_cmd+strlen(final_test_cmd),"%s",buffer);
    pthread_cond_signal(&final_test_cmd_cv);
    pthread_mutex_unlock(&final_test_cmd_lock);
  }else{
    // otherwise process the packet
    error = process_control_command(buffer);
  }
  // send response
  if (error != 0 && error != 999){
    // one of the sockets was locked already or no threads available
    if (FD_ISSET(fd,&main_writeable_fdset))
      write(fd,CONT_CMD_BSY,strlen(CONT_CMD_BSY));
    else
      printsend("Could not send response to controller - check connection\n");
  }else{
    // command was processed successfully
    if (FD_ISSET(fd,&main_writeable_fdset))
      write(fd,CONT_CMD_ACK,strlen(CONT_CMD_ACK));
    else
      printsend("Could not send response to controller - check connection\n");
  }
  return 0;
}

int read_viewer_data(int fd)
{
  int i;
  memset(buffer,'\0',MAX_PACKET_SIZE);
  int numbytes = recv(fd, buffer, MAX_PACKET_SIZE, 0);
  // check if theres any errors or an EOF packet
  if (numbytes < 0){
    printsend("Error receiving packet from viewer\n");
    pthread_mutex_lock(&main_fdset_lock);
    FD_CLR(fd,&view_fdset);
    FD_CLR(fd,&main_fdset);
    close(fd);
    pthread_mutex_unlock(&main_fdset_lock);
    for (i=0;i<views_connected;i++)
      if (rw_view_fd[i] == fd)
        rw_view_fd[i] = -1;
    views_connected--;
    return -1;
  }else if (numbytes == 0){
    printsend("Closing viewer connection.\n");
    pthread_mutex_lock(&main_fdset_lock);
    FD_CLR(fd,&view_fdset);
    FD_CLR(fd,&main_fdset);
    close(fd);
    pthread_mutex_unlock(&main_fdset_lock);
    for (i=0;i<views_connected;i++)
      if (rw_view_fd[i] == fd)
        rw_view_fd[i] = -1;
    views_connected--;
    return -1;
  }

}

int process_control_command(char *buffer)
{
  int result = 0;
  //_!_begin_commands_!_
  if (strncmp(buffer,"exit",4)==0){
    printsend("Exiting daq.\n");
    sigint_func(SIGINT);
  }else if (strncmp(buffer,"help",4) == 0){
    result = print_help(buffer);
  }else if (strncmp(buffer,"print_connected",10)==0){
    result = print_connected();
  }else if (strncmp(buffer,"stop_logging",12)==0){
    result = stop_logging();
  }else if (strncmp(buffer,"set_location",12)==0){
    result = set_location(buffer);
  }else if (strncmp(buffer,"start_logging",13)==0){
    result = start_logging();
  }else if (strncmp(buffer,"sbc_control",11)==0){
    result = sbc_control(buffer);
  }else if (strncmp(buffer,"clear_screen",12)==0){
    system("clear");
    pt_printsend("Cleared screen\n");
  }else if (strncmp(buffer,"reset_speed",11)==0){
    result = reset_speed();
  }else if (strncmp(buffer,"kill_threads",12)==0){
    result = kill_all_threads();
  }else if (strncmp(buffer,"run_macro",9)==0){
    result = run_macro_from_tut();
  }else if (strncmp(buffer,"stop_macro",10)==0){
    running_macro = 0;
  }else if (strncmp(buffer,"xr",2)==0){
    result = xrw(buffer,0);
  }else if (strncmp(buffer,"xw",2)==0){
    result = xrw(buffer,1);
  }else if (strncmp(buffer,"fr",2)==0){
    result = frw(buffer,0);
  }else if (strncmp(buffer,"fw",2)==0){
    result = frw(buffer,1);
  }else if (strncmp(buffer,"mr",2)==0){
    result = mrw(buffer,0);
  }else if (strncmp(buffer,"mw",2)==0){
    result = mrw(buffer,1);
  }else if (strncmp(buffer,"debugging_on",12)==0){
    result = debugging_mode(buffer,1);
  }else if (strncmp(buffer,"debugging_off",13)==0){
    result = debugging_mode(buffer,0);
  }else if (strncmp(buffer,"change_mode",11)==0){
    result = cmd_change_mode(buffer);
  }else if (strncmp(buffer,"crate_init",10)==0){
    result = crate_init(buffer);
  }else if (strncmp(buffer,"mtc_init",8)==0){
    result = mtc_init(buffer);
  }else if (strncmp(buffer,"sm_reset",8)==0){
    result = sm_reset(buffer);
  }else if (strncmp(buffer,"read_local_voltage",18)==0){
    result = read_local_voltage(buffer);
  }else if (strncmp(buffer,"hv_readback",11)==0){
    result = hv_readback(buffer);
  }else if (strncmp(buffer,"xl3_rw",6)==0){
    result = cmd_xl3_rw(buffer);
  }else if (strncmp(buffer,"xl3_queue_rw",13)==0){
    result = xl3_queue_rw(buffer);
  }else if (strncmp(buffer,"set_alarm_dac",13)==0){
    result = set_alarm_dac(buffer);
  }else if (strncmp(buffer,"load_dac",8)==0){
    result = cmd_load_dac(buffer);
  }else if (strncmp(buffer,"read_bundle",11)==0){
    result = read_bundle(buffer);
  }else if (strncmp(buffer,"setup_chinj",11)==0){
    result = cmd_setup_chinj(buffer);
  }else if (strncmp(buffer,"mtc_read",8)==0){
    result = mtc_read(buffer);
  }else if (strncmp(buffer,"mtc_write",9)==0){
    result = mtc_write(buffer);
  }else if (strncmp(buffer,"set_mtca_thresholds",14)==0){
    result = set_mtca_thresholds(buffer);
  }else if (strncmp(buffer,"set_gt_mask",11)==0){
    result = cmd_set_gt_mask(buffer);
  }else if (strncmp(buffer,"set_gt_crate_mask",17)==0){
    result = cmd_set_gt_crate_mask(buffer);
  }else if (strncmp(buffer,"set_ped_crate_mask",18)==0){
    result = cmd_set_ped_crate_mask(buffer);
  }else if (strncmp(buffer,"run_pedestals_end_mtc",21)==0){
    result = run_pedestals_end(buffer,1,0);
  }else if (strncmp(buffer,"run_pedestals_end_crate",23)==0){
    result = run_pedestals_end(buffer,0,1);
  }else if (strncmp(buffer,"run_pedestals_end",17)==0){
    result = run_pedestals_end(buffer,1,1);
  }else if (strncmp(buffer,"run_pedestals_mtc",17)==0){
    result = run_pedestals(buffer,1,0);
  }else if (strncmp(buffer,"run_pedestals_crate",19)==0){
    result = run_pedestals(buffer,0,1);
  }else if (strncmp(buffer,"run_pedestals",13)==0){
    result = run_pedestals(buffer,1,1);
  }else if (strncmp(buffer,"enable_pulser",13)==0){
    result = cmd_enable_pulser(buffer);
  }else if (strncmp(buffer,"disable_pulser",14)==0){
    result = cmd_disable_pulser(buffer);
  }else if (strncmp(buffer,"enable_pedestal",15)==0){
    result = cmd_enable_pedestal(buffer);
  }else if (strncmp(buffer,"disable_pedestal",16)==0){
    result = cmd_disable_pedestal(buffer);
  }else if (strncmp(buffer,"set_pulser_freq",14)==0){
    result = set_pulser_freq(buffer);
  }else if (strncmp(buffer,"send_softgt",11)==0){
    result = cmd_send_softgt(buffer);
  }else if (strncmp(buffer,"multi_softgt",12)==0){
    result = cmd_multi_softgt(buffer);
  }else if (strncmp(buffer,"trigger_scan",12)==0){
    result = trigger_scan(buffer);
  }else if (strncmp(buffer,"see_refl",8)==0){
    result = see_refl(buffer);
  }else if (strncmp(buffer,"fec_test",8)==0){
    result = fec_test(buffer);
  }else if (strncmp(buffer,"mem_test",8)==0){
    result = mem_test(buffer);
  }else if (strncmp(buffer,"vmon",4)==0){
    result = vmon(buffer);
  }else if (strncmp(buffer,"board_id",8)==0){
    result = board_id(buffer);
  }else if (strncmp(buffer,"ped_run",7)==0){
    result = ped_run(buffer);
  }else if (strncmp(buffer,"zdisc",5)==0){
    result = zdisc(buffer);
  }else if (strncmp(buffer,"crate_cbal",10)==0){
    result = crate_cbal(buffer);
  }else if (strncmp(buffer,"cgt_test",8)==0){
    result = cgt_test(buffer);
  }else if (strncmp(buffer,"cmos_m_gtvalid",14)==0){
    result = cmos_m_gtvalid(buffer);
  }else if (strncmp(buffer,"cald_test",9)==0){
    result = cald_test(buffer);
  }else if (strncmp(buffer,"get_ttot",8)==0){
    result = get_ttot(buffer);
  }else if (strncmp(buffer,"set_ttot",8)==0){
    result = set_ttot(buffer);
  }else if (strncmp(buffer,"fifo_test",9)==0){
    result = fifo_test(buffer);
  }else if (strncmp(buffer,"disc_check",10)==0){
    result = disc_check(buffer);
  }else if (strncmp(buffer,"mb_stability_test",17)==0){
    result = mb_stability_test(buffer);
  }else if (strncmp(buffer,"chinj_scan",10)==0){
    result = chinj_scan(buffer);
  }else if (strncmp(buffer,"final_test",10)==0){
    result = final_test(buffer);
  }else if (strncmp(buffer,"find_noise",10)==0){
    result = find_noise(buffer);
  }else if (strncmp(buffer,"ecal_to_fec",11)==0){
    result = ecal_to_fec(buffer);
  }else if (strncmp(buffer,"ecal",4)==0){
    result = ecal(buffer);
  }
  //_!_end_commands_!_
  else{
    printsend("not a valid command\n");
    result = 999;
  }
  return result;
}

int process_xl3_packet(char *buffer, int xl3num)
{
  XL3_Packet *packet = (XL3_Packet *) buffer;
  SwapShortBlock(&(packet->cmdHeader.packet_num),1);
  if (packet->cmdHeader.packet_type == MESSAGE_ID){
    pt_printsend("%s",packet->payload);
  }else if (packet->cmdHeader.packet_type == MEGA_BUNDLE_ID){
    store_mega_bundle();
  }else if (packet->cmdHeader.packet_type == SCREWED_ID){
    printf("got a screwed packet\n"); //FIXME
    handle_screwed_packet(packet,xl3num);
  }else if (packet->cmdHeader.packet_type == ERROR_ID){
    handle_error_packet(packet,xl3num);
  }else if (packet->cmdHeader.packet_type == PING_ID){
    packet->cmdHeader.packet_type = PONG_ID;
    int n = write(rw_xl3_fd[xl3num],(char *)packet,MAX_PACKET_SIZE);
    if (n < 0){
      pt_printsend("process_xl3_packet: Error writing to socket for pong.\n");
      return -1;
    }
  }else{
    // got the wrong type of ack packet
    pt_printsend("process_xl3_packet: Got unexpected packet type in main loop: %02x %08x\n",packet->cmdHeader.packet_type,*(uint32_t *) packet);
    pt_printsend("lock is %d\n",xl3_lock[xl3num]);
  } // end switch on packet type
  return 0;
}

int store_mega_bundle(XL3_Packet *packet)
{
  struct timeval start,end;
  if (megabundle_count == 0){
    gettimeofday(&start,0);
    start_time = start.tv_sec*1000000 + start.tv_usec;
    recv_bytes = 0;
    recv_fake_bytes = 0;
  }
  gettimeofday(&end,0);
  end_time = end.tv_sec*1000000 + end.tv_usec;
  long int avg_dt = end_time - start_time;
  int num_bundles = packet->cmdHeader.num_bundles;

  if (num_bundles == 0){

    MegaBundleHeader *megabh = (MegaBundleHeader *) packet->payload;
    SwapLongBlock(megabh,3);
    MiniBundleHeader *minibh = (MiniBundleHeader *) (packet->payload+sizeof(MegaBundleHeader));
    int num_longwords = megabh->info & 0xFFFFFF;
    int crate = (megabh->info & 0x1F000000) >> 24;
    SwapLongBlock(megabh+1,num_longwords);
    while (num_longwords > 0){
      if (minibh->info & 0x80000000){
        uint32_t passcur = *(uint32_t *) (minibh + 1);
      }else{
        num_bundles += (minibh->info & 0xFFFFFF)/3;
        int card = (minibh->info & 0x0F000000) >> 24;
      }
      int minisize = (minibh->info & 0xFFFFFF);
      minibh += 1+ minisize;
      num_longwords -= 1 + minisize;
    }
  }

  if (num_bundles > 0){
    recv_bytes += num_bundles*12;
    recv_fake_bytes += (120-num_bundles)*12;
    megabundle_count++;

    if (megabundle_count%BUNDLE_PRINT == 0){
      long int inst_dt = end_time - last_print_time;
      last_print_time = end_time;
      pt_printsend("recv average: %8.2f Mb/s \t d/dt: %8.2f Mb/s (%.1f %% fake) (gtid: %6x)\n",
          (float) (recv_bytes*8/(float)avg_dt),
          (float)(num_bundles*12*8*BUNDLE_PRINT/(float)inst_dt),
          recv_fake_bytes/(float)(BUNDLE_PRINT*120*12)*100.0,current_gtid);
      //    pt_printsend("recv %i \t %i \t %i \t %i \t average: %8.2f Mb/s \t d/dt: %8.2f Mb/s (%.1f %% fake)\n",
      //        megabundle_count,num_bundles,(int) recv_bytes, (int) avg_dt,
      //        (float) (recv_bytes*8/(float)avg_dt),
      //        (float)(num_bundles*12*8*BUNDLE_PRINT/(float)inst_dt),
      //        recv_fake_bytes/(float)(BUNDLE_PRINT*120*12)*100.0);
      recv_fake_bytes = 0;
    }
  }
  return 0;
}

int handle_error_packet(XL3_Packet *packet, int xl3num)
{
  pt_printsend("Got an error packet from XL3 #%d:\n",xl3num);
  error_packet_t *errors = (error_packet_t *) packet->payload;
  if (errors->cmd_in_rejected)
    pt_printsend("\tCommand in rejected\n");
  if (errors->transfer_error)
    pt_printsend("\tTransfer error: %d\n",errors->transfer_error);
  if (errors->xl3_data_avail_unknown)
    pt_printsend("\tXL3 data available unknown\n");
  if (errors->bundle_read_error)
    pt_printsend("\tBundle read error\n");
  if (errors->bundle_resync_error)
    pt_printsend("\tBundle resync error\n");
  int i;
  for (i=0;i<16;i++)
    if (errors->mem_level_unknown[i])
      pt_printsend("\tMemory level unknown FEC %d\n",i);

  return 0;
}

int handle_screwed_packet(XL3_Packet *packet, int xl3num)
{
  pt_printsend("Got a screwed packet from XL3 #%d:\n",xl3num);
  screwed_packet_t *errors = (screwed_packet_t *) packet->payload;
  int i;
  for (i=0;i<16;i++)
    if (errors->screwed[i])
      pt_printsend("\tFEC %d\n",i);

  return 0;
}

int inspect_bundles(XL3_Packet *packet)
{
  int num_bundles = packet->cmdHeader.num_bundles;
  PMTBundle *one_bundle = (PMTBundle *) packet->payload;
  int i;
  for (i=0;i<num_bundles;i++){
    SwapLongBlock(one_bundle,3);
    uint32_t gtid = UNPK_FEC_GT24_ID(&one_bundle->word1); 
    uint16_t qhs = UNPK_QHS(&one_bundle->word1);
    uint16_t qhl = UNPK_QHL(&one_bundle->word1);
    uint16_t qlx = UNPK_QLX(&one_bundle->word1);
    uint16_t tac = UNPK_TAC(&one_bundle->word1);
    uint16_t crate = UNPK_CRATE_ID(&one_bundle->word1);
    uint16_t board = UNPK_BOARD_ID(&one_bundle->word1);
    uint16_t channel = UNPK_CHANNEL_ID(&one_bundle->word1);
    if (((gtid > current_gtid) && ((gtid-current_gtid) > 0x10000)) || ((gtid < current_gtid) && (current_gtid-gtid) > 0x10000) || ((gtid & 0xFFF) == 0x0)){
      printf("CCC - %u %u %u - Bad gtid! Expecting ~%06x, got %06x\n",crate,board,channel,current_gtid,gtid);
    }else{
      current_gtid = gtid;
    }
    //if (qhs == 0x7FF || qhs == 0xFFF || qhl == 0x7FF || qhl == 0xFFF || qlx == 0x7FF || qlx == 0xFFF || tac == 0x7FF || tac == 0xFFF){
    //  printf("CCC - %u %u %u - Bad charge! %03x %03x %03x %03x\n",crate,board,channel,qhs,qhl,qlx,tac);
    //}
    if (crate > 18 || board > 15 || channel > 31){
      printf("CCC - %u %u %u - bad ccc!\n",crate,board,channel);
    }
    one_bundle++;
  }
  return 0;
}
