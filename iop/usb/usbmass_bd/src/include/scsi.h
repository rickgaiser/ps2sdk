#ifndef _SCSI_H
#define _SCSI_H


struct scsi_interface {
    void* priv;

	int (*get_max_lun)(struct scsi_interface* scsi);
	int (*scsi_cmd)(struct scsi_interface* scsi, const unsigned char *cmd, unsigned int cmd_len, unsigned char *data, unsigned int data_len, unsigned int data_wr);
};


int  scsi_init(void);
void scsi_connect(struct scsi_interface* scsi);
void scsi_disconnect(struct scsi_interface* scsi);


#endif
