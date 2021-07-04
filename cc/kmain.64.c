#include <types.h>
#include <helloworld.h>
#include <video.h>
#include <memory.h>
#include <memory/paging.h>
#include <memory/mmap.h>
#include <systeminfo.h>
#include <strings.h>
#include <cpu/interrupt.h>
#include <acpi.h>
#include <acpi/aml.h>
#include <pci.h>
#include <iterator.h>
#include <ports.h>
#include <apic.h>
#include <linkedlist.h>
#include <cpu.h>
#include <utils.h>
#include <device/kbd.h>
#include <diskio.h>

uint8_t kmain64() {
	memory_heap_t* heap = memory_create_heap_simple(0, 0);

	memory_set_default_heap(heap);

	memory_page_table_t* p4 = memory_paging_clone_pagetable();

	memory_paging_switch_table(p4);

	video_clear_screen();

	if(interrupt_init() != 0) {
		printf("CPU: Fatal cannot init interrupts\n");

		return -1;
	}

	char_t* data = hello_world();

	printf("%s\n", data);

	printf("memory map table\n");
	printf("base\t\tlength\t\ttype\n");
	for(size_t i = 0; i < SYSTEM_INFO->mmap_entry_count; i++) {
		printf("0x%08lx\t0x%08lx\t0x%04lx\t0x%x\n",
		       SYSTEM_INFO->mmap[i].base,
		       SYSTEM_INFO->mmap[i].length,
		       SYSTEM_INFO->mmap[i].type,
		       SYSTEM_INFO->mmap[i].acpi);

		if(SYSTEM_INFO->mmap[i].type == MEMORY_MMAP_TYPE_RESERVED || SYSTEM_INFO->mmap[i].type == MEMORY_MMAP_TYPE_ACPI) {
			uint64_t base = SYSTEM_INFO->mmap[i].base;
			uint64_t len = SYSTEM_INFO->mmap[i].length;

			printf("MMAP: adding page for address 0x%lx with length 0x%lx\n", base, len);

			memory_paging_page_type_t pt;
			uint64_t pl;

			while(len > 0) {
				pt = MEMORY_PAGING_PAGE_TYPE_4K;
				pl = MEMORY_PAGING_PAGE_LENGTH_4K;

				if(len >= MEMORY_PAGING_PAGE_LENGTH_2M) {
					pt = MEMORY_PAGING_PAGE_TYPE_2M;
					pl = MEMORY_PAGING_PAGE_LENGTH_2M;
				}

				memory_paging_add_page(base, base, pt);

				if(pl > len) {
					break;
				}

				base += pl;
				len -= pl;
			}
		}
	}

	if(SYSTEM_INFO->boot_type == SYSTEM_INFO_BOOT_TYPE_PXE) {
		printf("System booted from pxe\n");

		disk_slot_t* initrd = (disk_slot_t*)DISK_SLOT_PXE_INITRD_BASE;
		for(int8_t i = 0; i < DISK_SLOT_INITRD_MAX_COUNT; i++) {
			if(initrd->type == DISK_SLOT_TYPE_UNUSED) {
				break;
			} else if(initrd->type != DISK_SLOT_TYPE_PXEINITRD) {
				printf("PXEINITRD: Fatal unknown slot type\n");
				return -1;
			}

			printf("Initrd start: 0x%08x end: 0x%08x\n", initrd->start, initrd->end);

			initrd++;
		}
	} else {
		printf("System booted from disk\n");
	}

	acpi_xrsdp_descriptor_t* desc = acpi_find_xrsdp();
	if(desc == NULL) {
		printf("acpi header not found or incorrect checksum\n");
	} else{
		printf("acpi header is ok\n\0");

		acpi_sdt_header_t* madt = acpi_get_table(desc, "APIC");
		if(madt == NULL) {
			printf("can not find madt or incorrect checksum\n");
		} else {
			printf("madt is found\n");

			linkedlist_t apic_entries = acpi_get_apic_table_entries(madt);

			if(apic_init_apic(apic_entries) != 0) {
				printf("cannot enable apic\n");

				return -2;
			} else {
				printf("apic and ioapic enabled\n");
			}
		}

		acpi_table_fadt_t* fadt = (acpi_table_fadt_t*)acpi_get_table(desc, "FACP");

		if(fadt == NULL) {
			printf("fadt not found\n\0");

			return -1;
		}

		int8_t acpi_enabled = -1;

		if(fadt->smi_command_port == 0) {
			printf("acpi command port is 0. ");
			acpi_enabled = 0;
		}

		if(fadt->acpi_enable == 0 && fadt->acpi_disable == 0) {
			printf("acpi enable/disable is 0. ");
			acpi_enabled = 0;
		}

		uint16_t pm_1a_port = fadt->pm_1a_control_block_address_64bit.address;
		uint32_t pm_1a_value = inw(pm_1a_port);

		if((pm_1a_value & 0x1) == 0x1) {
			printf("pm 1a control block acpi en is setted");
			acpi_enabled = 0;
		}

		if(acpi_enabled != 0) {
			outb(fadt->smi_command_port, fadt->acpi_enable);

			while((inw(pm_1a_port) & 0x1) != 0x1);

			printf("acpi enabled");
		}

		printf("\n");

		printf("DSDT address 0x%08lx\n", fadt->dsdt_address_32bit);

		memory_paging_add_page(fadt->dsdt_address_32bit, fadt->dsdt_address_32bit, MEMORY_PAGING_PAGE_TYPE_2M);

		acpi_sdt_header_t* dsdt = (acpi_sdt_header_t*)((uint64_t)(fadt->dsdt_address_32bit));
		if(acpi_validate_checksum(dsdt) == 0) {
			printf("dsdt ok\n");

			uint64_t acpi_hs = 0x1000000;
			uint64_t acpi_he = 0x2000000;
			uint64_t step = 0x200000;

			for(uint64_t i = acpi_hs; i < acpi_he; i += step) {
				memory_paging_add_page(i, i, MEMORY_PAGING_PAGE_TYPE_2M);
			}

			memory_heap_t* acpi_heap = memory_create_heap_simple(acpi_hs, acpi_he);

			int64_t aml_size = dsdt->length - sizeof(acpi_sdt_header_t);
			uint8_t* aml = (uint8_t*)(dsdt + 1);
			acpi_aml_parser_context_t* pctx = acpi_aml_parser_context_create_with_heap(acpi_heap, dsdt->revision, aml, aml_size);

			if(pctx != NULL) {
				printf("aml parser ctx created\n");

				if(acpi_aml_parse(pctx) == 0) {
					printf("aml parsed\n");
				} else {
					printf("aml not parsed\n");
				}

			} else {
				printf("aml parser creation failed\n");
			}

		} else {
			printf("dsdt not ok\n");
		}

		acpi_table_mcfg_t* mcfg = (acpi_table_mcfg_t*)acpi_get_table(desc, "MCFG");

		if(mcfg == NULL) {
			printf("can not find mcfg or incorrect checksum\n");
		} else {
			printf("mcfg is found at 0x%08p\n", mcfg);


			uint64_t pci_hs = 0x1000000;
			uint64_t pci_he = 0x2000000;
			uint64_t step = 0x200000;

			for(uint64_t i = pci_hs; i < pci_he; i += step) {
				memory_paging_add_page(i, i, MEMORY_PAGING_PAGE_TYPE_2M);
			}

			memory_heap_t* pci_heap = memory_create_heap_simple(pci_hs, pci_he);
			memory_set_default_heap(pci_heap);

			iterator_t* iter = pci_iterator_create_with_heap(pci_heap, mcfg);

			while(iter->end_of_iterator(iter) != 0) {
				pci_dev_t* p = iter->get_item(iter);

				printf("pci dev found  %02x:%02x:%02x.%02x -> %04x:%04x -> %02x:%02x ",
				       p->group_number, p->bus_number, p->device_number, p->function_number,
				       p->pci_header->vendor_id, p->pci_header->device_id,
				       p->pci_header->class_code, p->pci_header->subclass_code);


				if(p->pci_header->header_type.header_type == PCI_HEADER_TYPE_GENERIC_DEVICE) {
					pci_generic_device_t* pg = (pci_generic_device_t*)p->pci_header;

					printf("int %02x:%02x ", pg->interrupt_line, pg->interrupt_pin);

					if(pg->common_header.status.capabilities_list) {
						printf("caps ");
						pci_capability_t* pci_cap = (pci_capability_t*)(((uint8_t*)pg) + pg->capabilities_pointer);


						while(pci_cap->capability_id != 0xFF) {
							printf("0x%x \n", pci_cap->capability_id );

							if(pci_cap->next_pointer == NULL) {
								break;
							}

							pci_cap = (pci_capability_t*)(((uint8_t*)pci_cap ) + pci_cap->next_pointer);
						}
					}
				}

				printf("\n");

				memory_free(p);

				iter = iter->next(iter);
			}
			iter->destroy(iter);

			memory_set_default_heap(heap);
		}
	}

	printf("signed print test: 150=%li\n", 150 );
	printf("signed print test: -150=%i\n", -150 );
	printf("signed print test: -150=%09i\n", -150 );
	printf("unsigned print test %lu %li\n", -2UL, -2UL);

	printf("128 bit tests\n");
	printf("sizes: int128 %i unit128 %i float32 %i float64 %i float128 %i\n",
	       sizeof(int128_t), sizeof(uint128_t),
	       sizeof(float32_t), sizeof(float64_t), sizeof(float128_t) );

	float32_t f1 = 15;
	float32_t f2 = 2;
	float32_t f3 = f1 / f2;
	printf("div res: %lf\n", f3);

	printf("printf test for floats: %lf %lf %.3lf\n", -123.4567891234, -123.456, -123.4567891234);

	printf("i32 %i\n", 1 );

	outl(0x0CD8, 0); // qemu acpi init emulation

	interrupt_irq_set_handler(0x1, &dev_kbd_isr);
	apic_ioapic_setup_irq(0x1,
	                      APIC_IOAPIC_INTERRUPT_ENABLED
	                      | APIC_IOAPIC_DELIVERY_MODE_FIXED | APIC_IOAPIC_DELIVERY_STATUS_RELAX
	                      | APIC_IOAPIC_DESTINATION_MODE_PHYSICAL
	                      | APIC_IOAPIC_TRIGGER_MODE_EDGE | APIC_IOAPIC_PIN_POLARITY_ACTIVE_HIGH);


	printf("tests completed!...\n");

	while(1); // endless loop for listening interrupts

	return 0;
}
