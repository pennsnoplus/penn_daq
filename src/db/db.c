#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "packet_types.h"
#include "db_types.h"

#include "main.h"
#include "pouch.h"
#include "json.h"
#include "db.h"

int parse_mtc(JsonNode* value,mtc_t* mtc)
{
    int i;
    JsonNode* mtcd = json_find_member(value,"mtcd");
    JsonNode* mtca  = json_find_member(value,"mtca");
    JsonNode* nhit = json_find_member(mtca,"nhit");
    JsonNode* esum = json_find_member(mtca,"esum");
    JsonNode* spare = json_find_member(mtca,"spare");

    mtc->mtcd.lockout_width = (int)json_get_number(json_find_member(mtcd,"lockout_width"));
    mtc->mtcd.pedestal_width = (int)json_get_number(json_find_member(mtcd,"pedestal_width"));
    mtc->mtcd.nhit100_lo_prescale = (int)json_get_number(json_find_member(mtcd,"nhit100_lo_prescale"));
    mtc->mtcd.pulser_period = (int)json_get_number(json_find_member(mtcd,"pulser_period"));
    mtc->mtcd.low10Mhz_clock = (int)json_get_number(json_find_member(mtcd,"low10Mhz_clock"));
    mtc->mtcd.high10Mhz_clock = (int)json_get_number(json_find_member(mtcd,"high10Mhz_clock"));
    mtc->mtcd.fine_slope = (float) json_get_number(json_find_member(mtcd,"fine_slope"));
    mtc->mtcd.min_delay_offset = (float) json_get_number(json_find_member(mtcd,"min_delay_offset"));
    mtc->mtcd.coarse_delay = (int)json_get_number(json_find_member(mtcd,"coarse_delay"));
    mtc->mtcd.fine_delay = (int)json_get_number(json_find_member(mtcd,"fine_delay"));
    mtc->mtcd.gt_mask = strtoul(json_get_string(json_find_member(mtcd,"gt_mask")),(char**) NULL, 16);
    mtc->mtcd.gt_crate_mask = strtoul(json_get_string(json_find_member(mtcd,"gt_crate_mask")),(char**) NULL, 16);
    mtc->mtcd.ped_crate_mask = strtoul(json_get_string(json_find_member(mtcd,"ped_crate_mask")),(char**) NULL, 16);
    mtc->mtcd.control_mask = strtoul(json_get_string(json_find_member(mtcd,"control_mask")),(char**) NULL, 16);

    for (i=0;i<6;i++){
        sprintf(mtc->mtca.triggers[i].id,"%s",json_get_string(json_find_element(json_find_member(nhit,"id"),i)));
        mtc->mtca.triggers[i].threshold = (int)json_get_number(json_find_element(json_find_member(nhit,"threshold"),i));
        mtc->mtca.triggers[i].mv_per_adc = (int)json_get_number(json_find_element(json_find_member(nhit,"mv_per_adc"),i));
        mtc->mtca.triggers[i].mv_per_hit = (int)json_get_number(json_find_element(json_find_member(nhit,"mv_per_hit"),i));
        mtc->mtca.triggers[i].dc_offset = (int)json_get_number(json_find_element(json_find_member(nhit,"dc_offset"),i));
    }
    for (i=0;i<4;i++){
        sprintf(mtc->mtca.triggers[i+6].id,"%s",json_get_string(json_find_element(json_find_member(esum,"id"),i)));
        mtc->mtca.triggers[i+6].threshold = (int)json_get_number(json_find_element(json_find_member(esum,"threshold"),i));
        mtc->mtca.triggers[i+6].mv_per_adc = (int)json_get_number(json_find_element(json_find_member(esum,"mv_per_adc"),i));
        mtc->mtca.triggers[i+6].mv_per_hit = (int)json_get_number(json_find_element(json_find_member(esum,"mv_per_hit"),i));
        mtc->mtca.triggers[i+6].dc_offset = (int)json_get_number(json_find_element(json_find_member(esum,"dc_offset"),i));
    }
    for (i=0;i<4;i++){
        sprintf(mtc->mtca.triggers[i+10].id,"%s",json_get_string(json_find_element(json_find_member(spare,"id"),i)));
        mtc->mtca.triggers[i+10].threshold = (int)json_get_number(json_find_element(json_find_member(spare,"threshold"),i));
        mtc->mtca.triggers[i+10].mv_per_adc = (int)json_get_number(json_find_element(json_find_member(spare,"mv_per_adc"),i));
        mtc->mtca.triggers[i+10].mv_per_hit = (int)json_get_number(json_find_element(json_find_member(spare,"mv_per_hit"),i));
        mtc->mtca.triggers[i+10].dc_offset = (int)json_get_number(json_find_element(json_find_member(spare,"dc_offset"),i));
    }
    return 0;
}

int parse_test(JsonNode* test,mb_t* mb,int cbal,int zdisc,int all)
{
    int j,k;
    if (cbal == 1 || all == 1){
        for (j=0;j<2;j++){
            for (k=0;k<32;k++){
                mb->vbal[j][k] = (int)json_get_number(json_find_element(json_find_element(json_find_member(test,"vbal"),j),k)); 
            }
        }
    }
    if (zdisc == 1 || all == 1){
        for (k=0;k<32;k++){
            mb->vthr[k] = (int)json_get_number(json_find_element(json_find_member(test,"vthr"),k)); 
        }
    }
    return 0;
}

int parse_fec_debug(JsonNode* value,mb_t* mb)
{
    int j,k;
    for (j=0;j<2;j++){
        for (k=0;k<32;k++){
            mb->vbal[j][k] = (int)json_get_number(json_find_element(json_find_element(json_find_member(value,"vbal"),j),k)); 
            //printsend("[%d,%d] - %d\n",j,k,mb->vbal[j][k]);
        }
    }
    for (k=0;k<32;k++){
        mb->vthr[k] = (int)json_get_number(json_find_element(json_find_member(value,"vthr"),k)); 
        //printsend("[%d] - %d\n",k,mb->vthr[k]);
    }
    for (j=0;j<8;j++){
        mb->tdisc.rmp[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(value,"tdisc"),"rmp"),j));	
        mb->tdisc.rmpup[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(value,"tdisc"),"rmpup"),j));	
        mb->tdisc.vsi[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(value,"tdisc"),"vsi"),j));	
        mb->tdisc.vli[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(value,"tdisc"),"vli"),j));	
        //printsend("[%d] - %d %d %d %d\n",j,mb->tdisc.rmp[j],mb_consts->tdisc.rmpup[j],mb_consts->tdisc.vsi[j],mb_consts->tdisc.vli[j]);
    }
    mb->tcmos.vmax = (int)json_get_number(json_find_member(json_find_member(value,"tcmos"),"vmax"));
    mb->tcmos.tacref = (int)json_get_number(json_find_member(json_find_member(value,"tcmos"),"tacref"));
    //printsend("%d %d\n",mb->tcmos.vmax,mb_consts->tcmos.tacref);
    for (j=0;j<2;j++){
        mb->tcmos.isetm[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(value,"tcmos"),"isetm"),j));
        mb->tcmos.iseta[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(value,"tcmos"),"iseta"),j));
        //printsend("[%d] - %d %d\n",j,mb->tcmos.isetm[j],mb_consts->tcmos.iseta[j]);
    }
    for (j=0;j<32;j++){
        mb->tcmos.tac_shift[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(value,"tcmos"),"tac_shift"),j));
        //printsend("[%d] - %d\n",j,mb->tcmos.tac_shift[j]);
    }
    mb->vint = (int)json_get_number(json_find_member(value,"vint"));
    mb->hvref = (int)json_get_number(json_find_member(value,"hvref"));
    //printsend("%d %d\n",mb->vres,mb_consts->hvref);
    for (j=0;j<32;j++){
        mb->tr100.mask[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(value,"tr100"),"mask"),j));
        mb->tr100.tdelay[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(value,"tr100"),"delay"),j));
        mb->tr20.mask[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(value,"tr20"),"mask"),j));
        mb->tr20.tdelay[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(value,"tr20"),"delay"),j));
        mb->tr20.twidth[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(value,"tr20"),"width"),j));
        //printsend("[%d] - %d %d %d %d %d\n",j,mb->tr100.mask[j],mb_consts->tr100.tdelay[j],mb_consts->tr20.tdelay[j],mb_consts->tr20.twidth[j]);
    }
    for (j=0;j<32;j++){
        mb->scmos[j] = (int)json_get_number(json_find_element(json_find_member(value,"scmos"),j));
        //printsend("[%d] - %d\n",j,mb->scmos[j]);
    }
    mb->disable_mask = 0x0;
    for (j=0;j<32;j++){
        if ((int)json_get_number(json_find_element(json_find_member(value,"chan_disable"),j)) != 0)
            mb->disable_mask |= (0x1<<j);
    }

    return 0;

}


int parse_fec_hw(JsonNode* value,mb_t* mb)
{
    int j,k;
    JsonNode* hw = json_find_member(value,"hw");
    mb->mb_id = strtoul(json_get_string(json_find_member(value,"board_id")),(char**)NULL,16);
    mb->dc_id[0] = strtoul(json_get_string(json_find_member(json_find_member(hw,"id"),"db0")),(char**)NULL,16);
    mb->dc_id[1] = strtoul(json_get_string(json_find_member(json_find_member(hw,"id"),"db1")),(char**)NULL,16);
    mb->dc_id[2] = strtoul(json_get_string(json_find_member(json_find_member(hw,"id"),"db2")),(char**)NULL,16);
    mb->dc_id[3] = strtoul(json_get_string(json_find_member(json_find_member(hw,"id"),"db3")),(char**)NULL,16);
    //printsend("%04x,%04x,%04x,%04x\n",mb->mb_id,mb_consts->dc_id[0],mb_consts->dc_id[1],mb_consts->dc_id[2],mb_consts->dc_id[3]);
    for (j=0;j<2;j++){
        for (k=0;k<32;k++){
            mb->vbal[j][k] = (int)json_get_number(json_find_element(json_find_element(json_find_member(hw,"vbal"),j),k)); 
            //printsend("[%d,%d] - %d\n",j,k,mb->vbal[j][k]);
        }
    }
    for (k=0;k<32;k++){
        mb->vthr[k] = (int)json_get_number(json_find_element(json_find_member(hw,"vthr"),k)); 
        //printsend("[%d] - %d\n",k,mb->vthr[k]);
    }
    for (j=0;j<8;j++){
        mb->tdisc.rmp[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(hw,"tdisc"),"rmp"),j));	
        mb->tdisc.rmpup[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(hw,"tdisc"),"rmpup"),j));	
        mb->tdisc.vsi[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(hw,"tdisc"),"vsi"),j));	
        mb->tdisc.vli[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(hw,"tdisc"),"vli"),j));	
        //printsend("[%d] - %d %d %d %d\n",j,mb->tdisc.rmp[j],mb_consts->tdisc.rmpup[j],mb_consts->tdisc.vsi[j],mb_consts->tdisc.vli[j]);
    }
    mb->tcmos.vmax = (int)json_get_number(json_find_member(json_find_member(hw,"tcmos"),"vmax"));
    mb->tcmos.tacref = (int)json_get_number(json_find_member(json_find_member(hw,"tcmos"),"tacref"));
    //printsend("%d %d\n",mb->tcmos.vmax,mb_consts->tcmos.tacref);
    for (j=0;j<2;j++){
        mb->tcmos.isetm[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(hw,"tcmos"),"isetm"),j));
        mb->tcmos.iseta[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(hw,"tcmos"),"iseta"),j));
        //printsend("[%d] - %d %d\n",j,mb->tcmos.isetm[j],mb_consts->tcmos.iseta[j]);
    }
    for (j=0;j<32;j++){
        mb->tcmos.tac_shift[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(hw,"tcmos"),"tac_shift"),j));
        //printsend("[%d] - %d\n",j,mb->tcmos.tac_shift[j]);
    }
    mb->vint = (int)json_get_number(json_find_member(hw,"vint"));
    mb->hvref = (int)json_get_number(json_find_member(hw,"hvref"));
    //printsend("%d %d\n",mb->vres,mb_consts->hvref);
    for (j=0;j<32;j++){
        mb->tr100.mask[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(hw,"tr100"),"mask"),j));
        mb->tr100.tdelay[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(hw,"tr100"),"delay"),j));
        mb->tr20.mask[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(hw,"tr20"),"mask"),j));
        mb->tr20.tdelay[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(hw,"tr20"),"delay"),j));
        mb->tr20.twidth[j] = (int)json_get_number(json_find_element(json_find_member(json_find_member(hw,"tr20"),"width"),j));
        //printsend("[%d] - %d %d %d %d %d\n",j,mb->tr100.mask[j],mb_consts->tr100.tdelay[j],mb_consts->tr20.tdelay[j],mb_consts->tr20.twidth[j]);
    }
    for (j=0;j<32;j++){
        mb->scmos[j] = (int)json_get_number(json_find_element(json_find_member(hw,"scmos"),j));
        //printsend("[%d] - %d\n",j,mb->scmos[j]);
    }
    mb->disable_mask = 0x0;
    for (j=0;j<32;j++){
        if ((int)json_get_number(json_find_element(json_find_member(hw,"chan_disable"),j)) != 0)
            mb->disable_mask |= (0x1<<j);
    }
    return 0;

}

int swap_fec_db(mb_t* mb)
{
    SwapShortBlock(&(mb->mb_id),1);
    SwapShortBlock((mb->dc_id),4);
    SwapShortBlock((mb->scmos),32);
    SwapLongBlock(&(mb->disable_mask),1);
    return 0;
}

int post_debug_doc(int crate, int card, JsonNode* doc)
{
    char mb_ids[8],dc_ids[4][8],hv_ids[8];
    char put_db_address[500];
    update_crate_config(crate,0x1<<card);
    time_t the_time;
    the_time = time(0); //
    char datetime[100];
    sprintf(datetime,"%s",(char *) ctime(&the_time));

    sprintf(mb_ids,"%04x",crate_config[crate][card].mb_id);
    sprintf(dc_ids[0],"%04x",crate_config[crate][card].dc_id[0]);
    sprintf(dc_ids[1],"%04x",crate_config[crate][card].dc_id[1]);
    sprintf(dc_ids[2],"%04x",crate_config[crate][card].dc_id[2]);
    sprintf(dc_ids[3],"%04x",crate_config[crate][card].dc_id[3]);
    json_append_member(doc,"db0_id",json_mkstring(dc_ids[0]));
    json_append_member(doc,"db1_id",json_mkstring(dc_ids[1]));
    json_append_member(doc,"db2_id",json_mkstring(dc_ids[2]));
    json_append_member(doc,"db3_id",json_mkstring(dc_ids[3]));
    json_append_member(doc,"mb_id",json_mkstring(mb_ids));
    sprintf(hv_ids,"%d",crate);
    json_append_member(doc,"crate",json_mkstring(hv_ids));
    sprintf(hv_ids,"%d",card);
    json_append_member(doc,"slot",json_mkstring(hv_ids));
    json_append_member(doc,"timestamp",json_mknumber((double)(long int) the_time));
    json_append_member(doc,"datetime",json_mkstring(datetime));
    json_append_member(doc,"location",json_mknumber((double)CURRENT_LOCATION));
    // TODO: this might be leaking a lot...
    sprintf(put_db_address,"http://%s:%s/%s",DB_ADDRESS,DB_PORT,DB_BASE_NAME);
    pouch_request *post_response = pr_init();
    pr_set_method(post_response, POST);
    pr_set_url(post_response, put_db_address);
    char *data = json_encode(doc);
    pr_set_data(post_response, data);
    pr_do(post_response);
    int ret = 0;
    if (post_response->httpresponse != 201){
       pt_printsend("error code %d\n",(int)post_response->httpresponse);
        ret = -1;
    }
    pr_free(post_response);
    if(*data){
        free(data);
    }
    return ret;
};

int post_debug_doc_with_id(int crate, int card, char *id, JsonNode* doc)
{
    char mb_ids[8],dc_ids[4][8],hv_ids[8];
    char put_db_address[500];
    update_crate_config(crate,0x1<<card);
    time_t the_time;
    the_time = time(0); //
    char datetime[100];
    sprintf(datetime,"%s",(char *) ctime(&the_time));

    sprintf(mb_ids,"%04x",crate_config[crate][card].mb_id);
    sprintf(dc_ids[0],"%04x",crate_config[crate][card].dc_id[0]);
    sprintf(dc_ids[1],"%04x",crate_config[crate][card].dc_id[1]);
    sprintf(dc_ids[2],"%04x",crate_config[crate][card].dc_id[2]);
    sprintf(dc_ids[3],"%04x",crate_config[crate][card].dc_id[3]);
    json_append_member(doc,"db0_id",json_mkstring(dc_ids[0]));
    json_append_member(doc,"db1_id",json_mkstring(dc_ids[1]));
    json_append_member(doc,"db2_id",json_mkstring(dc_ids[2]));
    json_append_member(doc,"db3_id",json_mkstring(dc_ids[3]));
    json_append_member(doc,"mb_id",json_mkstring(mb_ids));
    sprintf(hv_ids,"%d",crate);
    json_append_member(doc,"crate",json_mkstring(hv_ids));
    sprintf(hv_ids,"%d",card);
    json_append_member(doc,"slot",json_mkstring(hv_ids));
    json_append_member(doc,"timestamp",json_mknumber((double)(long int) the_time));
    json_append_member(doc,"datetime",json_mkstring(datetime));
    json_append_member(doc,"location",json_mknumber((double)CURRENT_LOCATION));

    sprintf(put_db_address,"http://%s:%s/%s/%s",DB_ADDRESS,DB_PORT,DB_BASE_NAME,id);
    pouch_request *post_response = pr_init();
    pr_set_method(post_response, PUT);
    pr_set_url(post_response, put_db_address);
    char *data = json_encode(doc);
    pr_set_data(post_response, data);
    pr_do(post_response);
    int ret = 0;
    if (post_response->httpresponse != 201){
       pt_printsend("error code %d\n",(int)post_response->httpresponse);
        ret = -1;
    }
    pr_free(post_response);
    if(*data){
        free(data);
    }
    return 0;
};
