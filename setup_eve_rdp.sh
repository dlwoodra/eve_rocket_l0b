#!/bin/bash
# need to call using   . setup_eve_rdp.sh
# if you then run tcsh it will inherit these environment variables


export eve_data_root=./data/
export eve_log_root=./logs/
export eve_code_root=./

export eve_data_record=${eve_data_root}record/
export eve_data_tlm=${eve_data_record}
export eve_data_l0b=${eve_data_root}level0b/
export eve_data_quicklook=${eve_data_root}quicklook/

export eve_cal_data=${eve_data_root}cal/

