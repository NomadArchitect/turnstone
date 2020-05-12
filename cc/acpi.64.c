#include <acpi.h>
#include <bios.h>
#include <memory.h>

acpi_xrsdp_descriptor_t* acpi_find_xrsdp(){
	bios_data_area_t* bda = (bios_data_area_t*)(BIOS_BDA_POINTER);
	uint8_t* ebda = (uint8_t*)((size_t)bda->ebda_base_address << 4);
	size_t ebda_size = ebda[0] * 1024;
	acpi_xrsdp_descriptor_t* desc = NULL;
	for(size_t i = 0; i < ebda_size; i += 16) {
		if(memory_memcompare(ebda + i, ACPI_RSDP_SIGNATURE, 8) == 0) {
			desc = (acpi_xrsdp_descriptor_t*)(ebda + i);
			break;
		}
	}
	if(desc == NULL) {
		for(uint8_t* rom = (uint8_t*)(0xE0000); rom < (uint8_t*)(0xFFFFF); rom += 16) {
			if(memory_memcompare(rom, ACPI_RSDP_SIGNATURE, 8) == 0) {
				desc = (acpi_xrsdp_descriptor_t*)rom;
				break;
			}
		}
	}
	if(desc != NULL) {
		if(desc->revision != 2) {
			return NULL;
		}
		uint8_t* data2csum = (uint8_t*)desc;
		uint8_t checksum = 0;
		for(size_t i = 0; i < desc->length; i++) {
			checksum += data2csum[i];
		}
		if(checksum != 0x00) {
			return NULL;
		}
		return desc;
	}

	return NULL;
}


uint8_t acpi_validate_checksum(acpi_sdt_header_t* sdt_header){
	uint8_t* data = (uint8_t*)sdt_header;
	uint8_t checksum = 0;
	for(size_t i = 0; i < sdt_header->length; i++) {
		checksum += data[i];
	}
	return checksum;
}

acpi_sdt_header_t* acpi_get_table(acpi_xrsdp_descriptor_t* xrsdp_desc, char_t* signature){
	size_t table_count = (xrsdp_desc->xrsdt->header.length - sizeof(acpi_sdt_header_t)) / sizeof(void*);
	acpi_sdt_header_t* res;
	for(size_t i = 0; i < table_count; i++) {
		res = xrsdp_desc->xrsdt->acpi_sdt_header_ptrs[i];
		if(memory_memcompare(res->signature, signature, 4) == 0) {
			if(acpi_validate_checksum(res) == 0) {
				return res;
			}
			return NULL;
		}
	}
	return NULL;
}


acpi_table_mcfg_t* acpi_get_mcfg_table(acpi_xrsdp_descriptor_t* xrsdp_desc){
	return (acpi_table_mcfg_t*)acpi_get_table(xrsdp_desc, "MCFG");
}