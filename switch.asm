section .text
global switch_task

; void switch_task(uint32_t *old_esp, uint32_t new_esp)
; Saves the callee-saved registers on the current stack, stores esp,
; loads the other task's esp, and restores its registers.
switch_task:
    mov eax, [esp + 4]      ; where to save the old esp
    mov edx, [esp + 8]      ; the new esp
    push ebp
    push ebx
    push esi
    push edi
    mov [eax], esp
    mov esp, edx
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret                     ; "returns" into the other task