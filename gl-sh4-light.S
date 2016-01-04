/* KallistiGL for KallistiOS ##version##

   libgl/gl-sh4-light.S
   Copyright (C) 2013-2014 Josh Pearson

   Dynamic Vertex Lighting
   
   This Assembly file contains 2 functions:
   _glKosSpotLight - This function computes diffuse / spotlight / attenuation
   _glKosSpecular - This functions computes the Specular Term
*/

!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

!float _glKosSpecular( void * vertex6f, void * eyepos, void * Lvectorin );
.globl __glKosSpecular

!r4 = [arg][void*] = vertex
!r5 = [arg][void*] = eyepos
!r6 = [arg][void*] = L vector

!fr0  = return value
!fv0  = vertex position (P)(N)
!fv4  = eye position    (E)
!fv8  = L vector        (L)

__glKosSpecular:
    
    fmov @r4+, fr0     ! load vertex x to fv0
    fmov @r4+, fr1     ! load vertex y to fv0
    fmov @r4+, fr2     ! load vertex z to fv0
	    
    fmov @r5+, fr4     ! load eye pos x to fv4
    fmov @r5+, fr5     ! load eye pos y to fv4
    fmov @r5+, fr6     ! load eye pos z to fv4 
	    
    fmov @r6+, fr8     ! load L vector x to fv8
    fmov @r6+, fr9     ! load L vector y to fv8 
    fmov @r6+, fr10    ! load L vector z to fv8
        
    fsub fr0, fr4      ! fv4 = V = normalize ( E - P )
    fsub fr1, fr5
    fsub fr2, fr6
    fldi0 fr3          ! load 0 for P w
    fldi0 fr7          ! load 0 for E w

    fipr fv4, fv4      ! Normalize V vector
    fsrra fr7
    fmul fr7, fr4
    fmul fr7, fr5
    fmul fr7, fr6    
    
    fadd fr4, fr8      ! fv8 = H = normalize( L + V )
    fadd fr5, fr9
    fadd fr6, fr10
    fldi0 fr11         ! load 0 for H w

    fipr fv8, fv8      ! Normalize H vector
    fsrra fr11
    fmul fr11, fr8
    fmul fr11, fr9
    fmul fr11, fr10

    fmov @r4+, fr0     ! load N to fv0
    fmov @r4+, fr1
    fmov @r4+, fr2
    
    fipr fv0, fv8      ! N dot H for specular term
	   
    rts
    fmov fr11, fr0     ! move N dot H to fr0 for return

!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

!int _glKosSpotlight( void * glLight, void * vertex6f, void * Lvectorout );
.globl __glKosSpotlight

!r0 = return value | true or false
!r1 = boolean flag
!r2 = boolean false
!r4 = [arg] = light
!r5 = [arg] = vertex
!r6 = [arg] = output

!fv0 = vertex position
!fv4 = light position  | L vector | normalize(light pos - vertex pos) | w = attenuation distance
!fv8 = light direction | N vector = Vertex Normal
!fv12 = Spot vector | normalize(vertex pos - light pos)

__glKosSpotlight:
    
    mov #0, r1         ! load 0 for boolean flag
    mov #0, r2         ! load 0 for boolean false
    
    fmov @r4+, fr4     ! load light position x to fv4
    fmov @r4+, fr5     ! load light position y to fv4
    fmov @r4+, fr6     ! load light position z to fv4
    fmov @r4+, fr7     ! load light position w to fv4
    
    fmov @r4+, fr8     ! load light direction x to fv8
    fmov @r4+, fr9     ! load light direction y to fv8
    fmov @r4+, fr10    ! load light direction z to fv8
    
    fmov @r5+, fr0     ! load vertex position x to fv0
    fmov @r5+, fr1     ! load vertex position y to fv0
    fmov @r5+, fr2     ! load vertex position z to fv0

    fldi0 fr11         ! load 0 for light dir w
    fcmp/gt fr11, fr7  ! light pos w component set = spot light
    bf .VERTEXLIGHT0   ! light not a spot light - branch to vertex lighting
    
    fschg
    fmov dr0, dr12     ! copy vertex x,y to fv12
    fschg
    fmov fr2, fr14     ! copy vertex z to fv12
    
    fsub fr4, fr12     ! fv12 = ( vertex position - light position)
    fsub fr5, fr13
    fsub fr6, fr14
    fldi0 fr15         ! set fv12 w component to 0
	
    fipr fv12, fv12    ! Normalize vector
    fsrra fr15
    fmul fr15, fr12
    fmul fr15, fr13
    fmul fr15, fr14
	 
    fldi0 fr15         ! set fv12 w component to 0
    fipr fv12, fv8     ! fr11 now holds light cosDir

    fmov @r4+, fr15    ! load light cutOff to fr15
    mov #1, r1         ! load 1 for boolean flag = indicate light cutOff was read
		
    fcmp/gt fr15, fr11 ! cosDir > cutOff ? 0 : 1
    bt .RET0spot       ! vertex outside of spotlight = return 0

.VERTEXLIGHT0:

    fsub fr0, fr4     ! fv4 = L vector = ( light position - vertex position)
    fsub fr1, fr5
    fsub fr2, fr6
    fldi0 fr7         ! load 0 for L w
    fldi0 fr11        ! load 0 for N w

    fipr fv4, fv4     ! Normalize L vector
    fsrra fr7
    fmul fr7, fr4
    fmul fr7, fr5
    fmul fr7, fr6     ! fv4 = Normalized L Vector
 	
    fmov fr7, fr3     ! copy L w to fr3 
	
    fldi0 fr7         ! load 0 for L w
	
    fcmp/gt fr7, fr3  
    bf .RET0spot      ! if L w < 0, return 0 now to avoid pointless computations
	
    fmov @r5+, fr8    ! load normal to fv8
    fmov @r5+, fr9    ! load normal to fv8
    fmov @r5+, fr10   ! load normal to fv8
	
    fipr fv8, fv4     ! N dot L
	
    fcmp/gt fr11, fr7 ! L w < = 0 ? L w = 0
    bf .RET0spot:     ! if L w < 0, return 0 now to avoid pointless computations
	    
.WRITEDi:
	
    fschg
    fmov dr4, @r6     ! write L vector x to output
    fschg
    add #8, r6
    fmov fr6, @r6     ! write L vector z to output
    add #4, r6
	    
    cmp/gt r2, r1
    bt .READattenf
    
    add #4, r4
    
.READattenf:
    
    fmov @r4+, fr8    ! vertex normal gets overwritten by atten factors
    fmov @r4+, fr9
    fmov @r4+, fr10
    fldi1 fr11
    
    fmul fr3, fr9     ! calculate attenuation
    fmul fr3, fr10
    fmul fr3, fr10
    fadd fr9, fr8
    fadd fr10, fr8
    fdiv fr8, fr11     ! fr11 = A

    fmul fr7, fr11     ! D * A
	
    fmov fr11, @r6     ! write D*A to output
			    
.RET1spot:

    rts
    mov #1, r0

.RET0spot:

    rts
    mov #0, r0

!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
