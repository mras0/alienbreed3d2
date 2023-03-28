;
; *****************************************************************************
; *
; * modules/rawkey_macros.i
; *
; * Raw Keycodes: Assumes UK layout in naming convention.
; *
; *****************************************************************************

; Zeroth Row
RAWKEY_ESC			EQU $45
RAWKEY_F1			EQU	$50
RAWKEY_F2			EQU	$51
RAWKEY_F3			EQU	$52
RAWKEY_F4			EQU	$53
RAWKEY_F5			EQU	$54
RAWKEY_F6			EQU	$55
RAWKEY_F7			EQU	$56
RAWKEY_F8			EQU	$57
RAWKEY_F9			EQU	$58
RAWKEY_F10			EQU	$59

; First Row
RAWKEY_BACKTICK		EQU $00
RAWKEY_1			EQU $01
RAWKEY_2			EQU $02
RAWKEY_3			EQU $03
RAWKEY_4			EQU $04
RAWKEY_5			EQU $05
RAWKEY_6			EQU $06
RAWKEY_7			EQU $07
RAWKEY_8			EQU $08
RAWKEY_9			EQU $09
RAWKEY_0			EQU $0A
RAWKEY_UNDERSCORE	EQU $0B
RAWKEY_EQUAL		EQU $0C
RAWKEY_BSLASH		EQU $0D
RAWKEY_BACKSPACE	EQU $41

; Second Row
RAWKEY_TAB			EQU $42
RAWKEY_Q			EQU $10
RAWKEY_W			EQU $11
RAWKEY_E			EQU $12
RAWKEY_R			EQU $13
RAWKEY_T			EQU $14
RAWKEY_Y			EQU $15
RAWKEY_U			EQU $16
RAWKEY_I			EQU $17
RAWKEY_O			EQU $18
RAWKEY_P			EQU $19
RAWKEY_LSQR_BRKT	EQU $1A
RAWKEY_RSQR_BRKT	EQU $1B
RAWKEY_ENTER		EQU $44

; Third Row
RAWKEY_CTRL			EQU $63
RAWKEY_CAPSLOCK		EQU $62
RAWKEY_A			EQU $20
RAWKEY_S			EQU $21
RAWKEY_D			EQU $22
RAWKEY_F			EQU $23
RAWKEY_G			EQU $24
RAWKEY_H			EQU $25
RAWKEY_J			EQU $26
RAWKEY_K			EQU $27
RAWKEY_L			EQU $28
RAWKEY_SEMICOLON	EQU $29
RAWKEY_HASH			EQU $2A
RAWKEY_RMYSTERY		EQU $2B

; Fourth Row
RAWKEY_LSHIFT		EQU	$60
RAWKEY_LMYSTERY		EQU $30
RAWKEY_Z			EQU $31
RAWKEY_X			EQU $32
RAWKEY_C			EQU $33
RAWKEY_V			EQU $34
RAWKEY_B			EQU $35
RAWKEY_N			EQU $36
RAWKEY_M			EQU $37
RAWKEY_COMMA		EQU $38
RAWKEY_DOT			EQU $39
RAWKEY_FSLASH		EQU $3A
RAWKEY_RSHIFT		EQU $61

; Fifth Row
RAWKEY_LALT			EQU $64
RAWKEY_LAMIGA		EQU $66
RAWKEY_SPACEBAR		EQU $40
RAWKEY_RAMIGA		EQU $67
RAWKEY_RALT			EQU $64 ; really?

; Cursor Block
RAWKEY_DEL			EQU $46
RAWKEY_HELP			EQU $5F
RAWKEY_UP			EQU $4C
RAWKEY_LEFT			EQU $4F
RAWKEY_DOWN			EQU $4D
RAWKEY_RIGHT		EQU $4E

; Numeric block
RAWKEY_NUM_LBRKT	EQU $5A
RAWKEY_NUM_RBRKT	EQU $5B
RAWKEY_NUM_FSLASH	EQU $5C
RAWKEY_NUM_ASTERISK	EQU $5D
RAWKEY_NUM_7		EQU $3D
RAWKEY_NUM_8		EQU $3E
RAWKEY_NUM_9		EQU $3F
RAWKEY_NUM_MINUS	EQU $4A
RAWKEY_NUM_4		EQU $2D
RAWKEY_NUM_5		EQU $2E
RAWKEY_NUM_6		EQU $2F
RAWKEY_NUM_PLUS		EQU $5E
RAWKEY_NUM_1		EQU $1D
RAWKEY_NUM_2		EQU $1E
RAWKEY_NUM_3		EQU $1F
RAWKEY_NUM_ENTER	EQU $43
RAWKEY_NUM_0		EQU $0F
RAWKEY_NUM_DOT		EQU $3C

