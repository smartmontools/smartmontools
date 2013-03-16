#ifndef CCISS_H_
#define CCISS_H_

#define CCISS_H_CVSID "$Id: cciss.h,v 1.1 2007/04/01 16:49:46 shattered Exp $\n"

int cciss_io_interface(int device, int target,
			      struct scsi_cmnd_io * iop, int report);

#endif /* CCISS_H_ */
