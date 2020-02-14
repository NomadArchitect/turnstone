#ifndef ___TYPES_H
#define ___TYPES_H 0

#if ___BITS == 16
asm (".code16gcc");
#endif

#define char_t char
#define int8_t char
#define uchar_t unsigned char
#define uint8_t unsigned char
#define int16_t short
#define uint16_t unsigned short

#if ___BITS == 16

typedef struct {
	int16_t part_low;
	int16_t part_high;
} __attribute__ ((packed)) int32_t;

typedef struct {
	uint16_t part_low;
	uint16_t part_high;
} __attribute__ ((packed)) uint32_t;

typedef struct {
	int32_t part_low;
	int32_t part_high;
} __attribute__ ((packed)) int64_t;

typedef struct {
	uint32_t part_low;
	uint32_t part_high;
} __attribute__ ((packed)) uint64_t;

#define reg_t uint16_t
#define size_t uint16_t
#define number_t int16_t

#elif ___BITS == 32

#define int32_t int
#define uint32_t unsigned int

typedef struct {
	int32_t part_low;
	int32_t part_high;
} __attribute__ ((packed)) int64_t;

typedef struct {
	uint32_t part_low;
	uint32_t part_high;
} __attribute__ ((packed)) uint64_t;


#define reg_t uint32_t
#define size_t uint32_t
#define number_t int32_t

#elif ___BITS == 64

#define int32_t int
#define uint32_t unsigned int
#define int64_t long
#define uint64_t unsigned long

#define reg_t uint64_t
#define size_t uint64_t
#define number_t int64_t

#endif


#endif
