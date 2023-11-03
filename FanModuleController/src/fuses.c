/*
 * fuses.c
 *
 * Created: 24.02.2021 11:08:09
 *  Author: E1130513
 */ 


#include <asf.h>

#include "fuses.h"
#include "config.h"

#ifndef BOOTLOADER

void program_fuses(void)
{
	uint32_t save_fuse_settings = 0;
	
	/* Make sure the NVM controller is ready */
	while (!(NVMCTRL->INTFLAG.reg & NVMCTRL_INTFLAG_READY));
	
	/* Auxiliary space cannot be accessed if the security bit is set */
	if (NVMCTRL->STATUS.reg & NVMCTRL_STATUS_SB) {
		return;
	}
	
	/* Read currently programmed fuses */
	if (*((uint32_t *)NVMCTRL_AUX0_ADDRESS) == CFG_FUSES_USER_WORD_0
			&& *(((uint32_t *)NVMCTRL_AUX0_ADDRESS) + 1) == CFG_FUSES_USER_WORD_1) {
		return;
	}
	
	printf("Programming fuses...\r\n");
	
	/* Disable Cache */
	save_fuse_settings = NVMCTRL->CTRLB.reg;
	
	NVMCTRL->CTRLB.reg = save_fuse_settings | NVMCTRL_CTRLB_CACHEDIS;
	
	/* Clear error flags */
	NVMCTRL->STATUS.reg |= NVMCTRL_STATUS_MASK;

	/* Set address, command will be issued elsewhere */
	NVMCTRL->ADDR.reg = NVMCTRL_AUX0_ADDRESS/2;
	
	/* Erase the user page */
	NVMCTRL->CTRLA.reg = NVM_COMMAND_ERASE_AUX_ROW | NVMCTRL_CTRLA_CMDEX_KEY;
	
	/* Wait for NVM command to complete */
	while (!(NVMCTRL->INTFLAG.reg & NVMCTRL_INTFLAG_READY));
	
	/* Clear error flags */
	NVMCTRL->STATUS.reg |= NVMCTRL_STATUS_MASK;
	
	/* Set address, command will be issued elsewhere */
	NVMCTRL->ADDR.reg = NVMCTRL_AUX0_ADDRESS/2;
	
	/* Erase the page buffer before buffering new data */
	NVMCTRL->CTRLA.reg = NVM_COMMAND_PAGE_BUFFER_CLEAR | NVMCTRL_CTRLA_CMDEX_KEY;

	/* Wait for NVM command to complete */
	while (!(NVMCTRL->INTFLAG.reg & NVMCTRL_INTFLAG_READY));
	
	/* Clear error flags */
	NVMCTRL->STATUS.reg |= NVMCTRL_STATUS_MASK;
	
	/* Set address, command will be issued elsewhere */
	NVMCTRL->ADDR.reg = NVMCTRL_AUX0_ADDRESS/2;
	
	*((uint32_t *)NVMCTRL_AUX0_ADDRESS) = CFG_FUSES_USER_WORD_0;
	*(((uint32_t *)NVMCTRL_AUX0_ADDRESS) + 1) = CFG_FUSES_USER_WORD_1;
	
	/* Write the user page */
	NVMCTRL->CTRLA.reg = NVM_COMMAND_WRITE_AUX_ROW | NVMCTRL_CTRLA_CMDEX_KEY;
	
	/* Restore the settings */
	NVMCTRL->CTRLB.reg = save_fuse_settings;
	
	/* Trigger a system reset for the changes to take effect */
	system_reset();
}

#endif /* BOOTLOADER */