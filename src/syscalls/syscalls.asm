.data
    PUBLIC currentSSN
    currentSSN DD 0

.code

DoSyscall PROC
    mov r10, rcx
    mov eax, dword ptr [currentSSN]
    syscall
    ret
DoSyscall ENDP

END
