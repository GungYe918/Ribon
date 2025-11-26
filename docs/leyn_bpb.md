# Leyn BPB(Leyn Boot Profile Binary) 설계 문서

## 1. 개요와 철학

Leyn BPB는 **Ribon 부트로더 -> Leyn 하이브리드 커널** 사이의 유일한 “부트 계약(boot contract)”이다.

전통적인 Multiboot2는 “태그 리스트” 형식의 느슨한 구조를 제공하지만,

* UEFI GOP / EFI 메모리 맵 / ACPI / SMBIOS / 네트워크 부트 정보 등 현대 펌웨어 정보 전체를 포괄하기에는 빈약하고,
* UEFI 기반의 고급 하이브리드/RTOS 설계(IPC 프로파일·모듈 정책·튜닝 파라미터까지 포함)를 표현하기엔 확장성이 제한적이다.

Leyn BPB의 목표는:

1. **Multiboot2 boot information이 제공하는 정보는 “기본값”으로 모두 표현 가능**
2. UEFI GOP, EFI System Table, EFI Memory Map, ACPI, SMBIOS, 네트워크(DHCP) 등
   **UEFI가 제공할 수 있는 모든 부트 환경 정보를 하나의 이진 포맷으로 통합**
3. TOML/DTS/모듈 정책/IPC 튜닝 등 **“운영체제 정책과 설정까지” 부트 시점에 전달**
4. 리얼타임·하이브리드 커널에 맞게 **핫패스에 필요한 정보만 최소 비용으로 접근 가능하도록 캐시 친화적 설계**
5. 메모리가 매우 적은 환경에서도 **“CORE ONLY” 모드로 최소 섹션만으로 부팅 가능**

를 모두 만족하는 것이다.

이를 위해 BPB는:

* **고정 헤더 + 섹션 테이블 + 섹션 페이로드**
* 각 섹션은 **type + flags + alignment + payload_offset + size**
* 섹션 플래그로 **CORE/OPTIONAL/HOT/COLD**, 출처(EFI/BIOS/FDT), 압축(LZ4), 수명(RUNTIME/DISCARDABLE/PERSIST) 등을 표현
* 설정/정책은 **CONFIG_GRAPH**라는 키–값 그래프 + BLOB 구조로 묶어 전달

하는 구조를 갖는다.

---

## 2. 전체 구조

### 2.1 메모리 상 레이아웃

BPB는 메모리 상에서 다음과 같은 레이아웃을 가진다:

```text
+-------------------------------+
| leyn_bpb_header               |
+-------------------------------+
| leyn_bpb_section section[]    |  // section_count 개
+-------------------------------+
| payload of section 0          |
+-------------------------------+
| payload of section 1          |
+-------------------------------+
| ...                           |
+-------------------------------+
```

* `header.total_size`
  전체 BPB 크기 (헤더 + 섹션 테이블 + 모든 페이로드)
* `header.section_count`
  섹션 개수
* 각 `leyn_bpb_section`:

  * `type` : 섹션의 종류 (FRAMEBUFFER_INFO, MEMORY_MAP, CONFIG_GRAPH 등)
  * `flags`: 이 섹션의 성격/출처/수명/압축 여부
  * `alignment`: 페이로드 정렬 요구(2의 제곱 권장)
  * `payload_offset`: BPB 시작 주소로부터 페이로드 시작까지의 바이트 오프셋
  * `payload_size`: (압축된) 페이로드 크기
  * `uncompressed_size`: 압축 해제 후 크기
  * `aux_data`: 타입별 추가 정보용 확장 필드

정렬 힌트를 위한 매크로:

* `LEYN_BPB_RECOMMENDED_ALIGN` = 64
* `LEYN_BPB_SECTION_TABLE_ALIGN` = 64
* `LEYN_BPB_SECTION_PAYLOAD_ALIGN` = 64

Ribon이 BPB를 만들 때:

* 헤더와 섹션 테이블을 64바이트 정렬
* CORE + HOT 섹션을 앞쪽에 몰아 배치하면 캐시 친화적이다.

### 2.2 헤더 철학

`leyn_bpb_header`는 다음을 캡슐화한다:

* **버전 관리**

  * `LEYN_BPB_MAGIC = 'LBPB'`
  * `version_major`, `version_minor`
* **타깃 아키텍처**

  * `arch` (x86_64, aarch64, riscv64 등)
* **크기 정보**

  * `header_size`, `total_size`, `section_count`
* **전역 플래그**

  * 엔디안, 64비트 주소 사용 여부
  * CORE ONLY 부팅 여부
  * 특정 섹션이 “필수(required)”인지 여부
  * UEFI 부팅인지 레거시 부팅인지
  * ACPI/SMBIOS/EFI 관련 섹션 존재 여부
  * 커널 엔트리가 물리/가상 주소인지
* **엔트리 포인트**

  * `kernel_entry_vaddr`
  * `LEYN_BPB_FLAG_ENTRY_PHYS_VALID` / `ENTRY_VIRT_VALID` 비트로 의미 지정
* **체크섬**

  * `checksum` + `FLAG_HAS_CHECKSUM`로, BPB 전체의 무결성 검증 가능

이 헤더 하나로 **BPB 이미지 자체의 ABI 버전 + 타깃 + 전체 크기/구성**을 빠르게 판단할 수 있다.

---

## 3. 부팅 흐름: UEFI -> Ribon -> Leyn

### 3.1 UEFI 부팅 경로(주경로)

1. **UEFI 펌웨어**

   * 플랫폼 초기화
   * UEFI 드라이버/부트 서비스 활성화
   * Ribon EFI 부트로더를 `EFI_BOOT_SERVICES->LoadImage/StartImage` 등으로 실행

2. **Ribon 부트로더**

   * `EFI_SYSTEM_TABLE`, `EFI_BOOT_SERVICES`, `EFI_RUNTIME_SERVICES` 포인터 획득
   * EFI 서비스 호출을 통해 다음 정보를 수집:

     * EFI Graphics Output Protocol(GOP) 모드 정보 및 프레임버퍼 주소
     * EFI 메모리 맵
     * ACPI RSDP(XSDP)
     * SMBIOS 테이블
     * 네트워크/DHCP 정보 (PXE·HTTP 부트 등일 경우)
     * 자체적으로 로드한 커널/모듈 이미지 위치
   * 이 정보들을 **정규화된 BPB 섹션**으로 재구성:

     * `LEYN_BPB_SEC_FRAMEBUFFER_INFO`
     * `LEYN_BPB_SEC_UEFI_GOP_MODE`
     * `LEYN_BPB_SEC_MEMORY_MAP` + `LEYN_BPB_SEC_EFI_MEMORY_MAP`
     * `LEYN_BPB_SEC_ACPI_RSDP_V1` / `_V2`
     * `LEYN_BPB_SEC_SMBIOS`
     * `LEYN_BPB_SEC_NETWORK_DHCP`
     * `LEYN_BPB_SEC_BOOT_MODULES`, `LEYN_BPB_SEC_KERNEL_IMAGE`, `LEYN_BPB_SEC_IMAGE_LOAD_BASE`
     * `LEYN_BPB_SEC_CONFIG_GRAPH` 등
   * 각 섹션에:

     * `LEYN_BPB_SEC_FLAG_SRC_EFI`
     * 필요 시 `CORE/OPTIONAL`, `HOT/COLD`, `COMP_LZ4` 등 플래그 세팅
   * BPB 전체 레이아웃을 구성하고 `header` 필드 채움

3. **ExitBootServices() 호출 여부**

   * 하이브리드 설계에 따라 Ribon이:

     * **이미 ExitBootServices() 호출 후** BPB를 넘길 수도 있고,
     * 아직 호출하지 않은 상태에서 넘겨서 Leyn이 직접 호출하도록 할 수도 있다.
   * 현재 상태는 `LEYN_BPB_FLAG_EFI_BOOT_SERVICES_ON` 플래그로 전달

4. **Ribon -> Leyn 엔트리 호칭**

   * Ribon은 `kernel_entry_vaddr`(또는 phys)를 준비하고,
     레지스터/스택 등 Leyn 규약(ABI)에 따라 **BPB의 시작 주소를 인자로 전달**한 뒤 점프한다.
   * Leyn 커널 초기 코드는:

     1. 매직/버전/아키텍처 확인
     2. 섹션 테이블을 순회하며 `type`/`flags` 기반으로 필요한 섹션을 찾는다.
     3. CORE + HOT 섹션(MEMORY_MAP, CORE_INFO, FRAMEBUFFER_INFO, CONFIG_GRAPH 메타 등)을 먼저 파싱한다.

### 3.2 BIOS/레거시 경로

BIOS만 있는 시스템(또는 QEMU 레거시 모드)에서는:

* VBE 정보, VGA 텍스트 모드 등은 BIOS 인터럽트로 얻고,

  * `LEYN_BPB_SEC_VBE_INFO` (`SRC_BIOS`)
  * `LEYN_BPB_SEC_VGA_TEXT_MODE` (`SRC_BIOS`)
* 메모리 맵은 e820 형식으로 읽어 `LEYN_BPB_SEC_MEMORY_MAP`에 정규화
* ACPI, SMBIOS도 BIOS가 제공하는 물리 주소를 통해 얻어 섹션으로 저장

이 경우 `LEYN_BPB_FLAG_LEGACY_BOOT`가 세트되고,
EFI 관련 섹션/플래그는 비활성화된다.

---

## 4. 메모리 및 기본 부트 정보 섹션

### 4.1 BASIC_MEMINFO

`leyn_bpb_basic_meminfo`:

* `mem_lower_kb`: 0~640KB 영역
* `mem_upper_kb`: 1MB 위의 첫 usable 영역까지

Multiboot2의 `mem_lower` / `mem_upper` 태그와 1:1 대응한다.

초기 매우 단순한 메모리 관리 코드에서
“대충 RAM 용량”이 필요할 때 빠르게 사용할 수 있다.

### 4.2 MEMORY_MAP

`leyn_bpb_mmap_entry` 배열로 구성된 `LEYN_BPB_SEC_MEMORY_MAP`은
Multiboot2 메모리 맵 태그와 ACPI e820 스타일을 모두 커버하는 **정규화된 메모리 맵**이다.

* `base_addr`, `length`: 물리 주소와 크기
* `type`: USABLE, ACPI, NVS, BAD 등
* `flags`: 향후 확장용

Leyn 커널의 물리 메모리 관리자(프레임 할당기)는
이 섹션만 보고 시스템의 모든 RAM/예약 영역을 파악할 수 있다.

### 4.3 BOOT_DEVICE_BIOS

`leyn_bpb_boot_device_bios`:

* Multiboot2 BIOS boot device 태그에 대응
* 실제 부팅에 사용된 BIOS 드라이브 번호(0x80 등)와 파티션 정보 기록
* 레거시 부팅에서만 의미 있으므로 `SRC_BIOS` 플래그를 붙이는 것이 자연스럽다.

### 4.4 IMAGE_LOAD_BASE & BOOT_MODULES

* `LEYN_BPB_SEC_IMAGE_LOAD_BASE`:

  * 커널 이미지의 로드 베이스 물리 주소
  * Multiboot2의 “Image Load Phys Addr Tag”에 해당

* `LEYN_BPB_SEC_BOOT_MODULES` + `leyn_bpb_boot_module[]`:

  * 여러 개의 initrd/추가 모듈을 동시에 전달
  * 각 모듈은:

    * `phys_start` / `phys_end`
    * 문자열 테이블 오프셋 (모듈 설명 혹은 경로)
    * 모듈 플래그(필수/옵션, 타입 등)

Multiboot2의 module tag 여러 개를 그대로 묶어서 표현한 것과 동일한 역할을 한다.

---

## 5. 프레임버퍼 & 콘솔: UEFI GOP + VBE + VGA 통합 모델

### 5.1 leyn_bpb_fb_info: 공통 프레임버퍼 서술자

`LEYN_BPB_SEC_FRAMEBUFFER_INFO` 섹션의 페이로드는 `leyn_bpb_fb_info`이며, 이는 Multiboot2 framebuffer tag의 일반화된 형태다.

필드:

* `framebuffer_addr` : 프레임버퍼 **물리 주소**
* `framebuffer_pitch`: 한 줄(스캔라인) 당 바이트 수
* `framebuffer_width`: 가로 픽셀(또는 문자 수)
* `framebuffer_height`: 세로 픽셀(또는 문자 수)
* `framebuffer_bpp`: 픽셀 당 비트 수 (텍스트 모드는 16)
* `framebuffer_type`:

  * `INDEXED` : 팔레트 인덱스 모드
  * `RGB`     : Direct RGB 마스크 모드
  * `TEXT`    : 텍스트 모드(VGA 등의 문자 셀)
* `backend`:

  * `UEFI_GOP`, `VBE`, `VGA_TEXT`, …
    -> 이 값만 보고도 “어떤 펌웨어 경로에서 왔는지” 직관적으로 구분 가능
* `color` union:

  * indexed 모드일 경우:

    * `num_colors`, `palette_offset`
  * RGB 모드일 경우:

    * `leyn_bpb_fb_rgb_info` (R/G/B 마스크 위치와 길이)

이 한 구조체만으로:

* 커널이 직접 framebuffer에 렌더링 가능
* 픽셀 포맷(RGB/BGR 순서, 마스크 길이)을 정확히 해석 가능

Multiboot2의 framebuffer 태그 구조가 사실상 같은 필드를 가지므로(주소, pitch, width/height, bpp, type, RGB 마스크)
GRUB 기반 부트로더의 정보를 Leyn BPB에 옮겨 담기도 쉽다.

### 5.2 UEFI GOP 매핑

UEFI Graphics Output Protocol은 `EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE` 구조체를 통해
프레임버퍼 해상도, 픽셀 포맷, FrameBufferBase, FrameBufferSize 등을 제공한다.

대표적인 필드:

* `HorizontalResolution`, `VerticalResolution`
* `PixelsPerScanLine`
* `PixelFormat` (RGB, BGR, BitMask 등)
* `FrameBufferBase`, `FrameBufferSize`

Ribon은 이를 다음과 같이 매핑한다:

* `framebuffer_addr`   ← `FrameBufferBase`
* `framebuffer_pitch`  ← `PixelsPerScanLine * (bpp / 8)`
* `framebuffer_width`  ← `HorizontalResolution`
* `framebuffer_height` ← `VerticalResolution`
* `framebuffer_bpp`    ← PixelFormat에 따라 산출 (보통 32)
* `backend`            ← `LEYN_BPB_FB_BACKEND_UEFI_GOP`
* `framebuffer_type`   ← 대부분 RGB
* RGB 마스크:

  * `PixelFormat`이 BitMask일 때 마스크 정보를 `leyn_bpb_fb_rgb_info`에 채운다.
  * 그렇지 않으면 표준 8:8:8 RGB/BGR 패턴으로 간주하고 미리 정의된 마스크를 사용.

추가로, GOP 고유 정보를 그대로 보존하고 싶을 때는
`LEYN_BPB_SEC_UEFI_GOP_MODE`에 `leyn_bpb_uefi_gop_mode`를 채워 넣는다.

* `HorizontalResolution`, `VerticalResolution`, `PixelsPerScanLine`
* `PixelFormat`
* `FrameBufferBase`, `FrameBufferSize`
* 선택적으로 mode info 구조체의 물리 주소(`mode_info_phys`)

이렇게 하면 커널 쪽에서는:

* 일반적인 렌더링 코드 -> `leyn_bpb_fb_info`만 사용
* UEFI 모드별 세부 사항이 필요하면 -> `UEFI_GOP_MODE` 섹션도 함께 참조

하는 2단계 설계가 가능하다.

### 5.3 VBE(레거시) 매핑

VESA VBE는 Multiboot2에서도 별도의 VBE info tag로 전달되는 수준일 뿐,
framebuffer 구조 자체는 Multiboot2의 framebuffer 태그로 통합된다.

Leyn BPB에서는:

* `LEYN_BPB_SEC_VBE_INFO` 페이로드에:

  * `vbe_control_info[512]`, `vbe_mode_info[256]`, `vbe_mode`, 인터페이스 세그먼트/오프셋 등
  * Multiboot2의 VBE 태그와 사실상 동일한 정보를 저장
* 동시에 해당 모드의 framebuffer를 `leyn_bpb_fb_info` 한 벌로 정규화해서:

  * `backend = VBE`
  * pitch, width/height, bpp, type, RGB 마스크 등을 `vbe_mode_info`로부터 채운다.

이로써 커널 쪽에서는 GOP와 VBE를 동일한 렌더링 경로에서 처리할 수 있다.

### 5.4 VGA 텍스트 모드

純 VGA 텍스트 모드 환경에서는 그래픽 모드가 아닌 문자 셀 기반의 화면이 제공된다.

* `LEYN_BPB_SEC_VGA_TEXT_MODE`:

  * `columns`, `rows`, `char_height`, `text_buffer_phys`
* `LEYN_BPB_SEC_FRAMEBUFFER_INFO`는:

  * `framebuffer_type = TEXT`
  * `backend = VGA_TEXT`
  * `framebuffer_width = columns`, `height = rows`
  * `framebuffer_addr = text_buffer_phys`
  * `framebuffer_bpp = 16` (문자 + 속성 2바이트)

이렇게 하면 커널은:

* 텍스트 모드에서도 동일한 구조체를 통해 화면 버퍼를 접근할 수 있고,
* GUI가 필요할 경우 문자열 렌더링 래퍼만 쓰면 된다.

---

## 6. UEFI / 펌웨어 관련 섹션

Leyn BPB는 UEFI·ACPI·SMBIOS·네트워크 정보를 모두 “섹션”으로 전달하며,
각 섹션은 **출처 플래그**로 UEFI/BIOS/FDT를 구분한다.

주요 섹션:

* `LEYN_BPB_SEC_EFI_SYSTEM_TABLE` + `leyn_bpb_efi_system_table_ptr`

  * `system_table_phys`: EFI_SYSTEM_TABLE* (물리 주소)
* `LEYN_BPB_SEC_EFI_IMAGE_HANDLE` + `leyn_bpb_efi_image_handle_ptr`

  * Ribon 이미지 핸들
* `LEYN_BPB_SEC_EFI_MEMORY_MAP` + `leyn_bpb_efi_mmap_meta`

  * descriptor_size, descriptor_version
  * 실제 엔트리 배열은 페이로드에 EFI 규약대로 저장
* `LEYN_BPB_SEC_ACPI_RSDP_V1 / _V2`

  * RSDP/XSDP 테이블 원본 전체
* `LEYN_BPB_SEC_SMBIOS`

  * SMBIOS 테이블 복사본
* `LEYN_BPB_SEC_NETWORK_DHCP` + `leyn_bpb_net_dhcp_meta`

  * 인터페이스 인덱스, DHCP ACK 패킷 원본

전역 플래그:

* `LEYN_BPB_FLAG_HAS_ACPI`
* `LEYN_BPB_FLAG_HAS_SMBIOS`
* `LEYN_BPB_FLAG_HAS_EFI_SYSTEM_TABLE`
* `LEYN_BPB_FLAG_HAS_EFI_IMAGE_HANDLE`
* `LEYN_BPB_FLAG_EFI_BOOT_SERVICES_ON`
* `LEYN_BPB_FLAG_HAS_NETWORK`

이 플래그로 “어떤 섹션이 반드시 존재할 것인지”를 빠르게 체크할 수 있다.

---

## 7. CONFIG_GRAPH: 설정·정책 그래프

### 7.1 구조 개요

`LEYN_BPB_SEC_CONFIG_GRAPH`는 **부트 설정 전체를 표현하는 핵심 섹션**이다.

페이로드 레이아웃:

```text
[ leyn_bpb_cfg_meta ]
[ leyn_bpb_cfg_node node_array[node_count] ]
[ string table (연속된 '\0' 종료 문자열들) ]
[ blob area (leyn_bpb_cfg_blob_header + payload + tail_magic ...) ]
```

* `leyn_bpb_cfg_meta`:

  * `node_array_offset`, `node_count`
  * `string_table_offset`, `string_table_size`
  * `blob_area_offset`, `blob_area_size`

### 7.2 노드(leyn_bpb_cfg_node)

각 노드는 **“키 1개 + 값 1개”**를 표현한다.

필드:

* `next_index`:

  * 단일 링크드 리스트처럼 연결
  * 0xFFFFFFFFu이면 끝
  * 배열 인덱스를 사용하므로 순차 접근이 캐시 친화적
* `value_type` (`leyn_bpb_cfg_value_type`):

  * `INT`, `UINT`, `BOOL`, `FLOAT`, `STRING`, `BLOB`, `OBJECT`, `ARRAY`, `REF`
* `flags` (`leyn_bpb_cfg_node_flags`):

  * `READONLY` : 런타임에서 변경 불가
  * `SECURE`   : 서명 검증/보안 채널을 통해서만 변경
  * `RUNTIME`  : 런타임 튜닝 파라미터
  * `DEPRECATED`, `INTERNAL`, `HOT` 등
* `key_offset`:

  * 문자열 테이블 기준 오프셋
  * 예: `"ipc.default_policy"`, `"modules.uart0.path"` 등
* `value` union:

  * `i64_value`, `u64_value`, `f64_value`
  * 타입에 따라 해석 (STRING/BLOB/OBJECT/ARRAY/REF는 오프셋/인덱스)

예시:

* `value_type = STRING, value.u64_value = 0x100`
  -> string table의 offset 0x100에 키의 값 문자열이 있음
* `value_type = OBJECT, value.u64_value = child_node_index`
  -> 트리 구조로 하위 노드 집합 관리
* `value_type = ARRAY, value.u64_value = array_start_index`
  -> 연속 노드 구간을 하나의 배열로 해석
* `value_type = REF, value.u64_value = target_node_index`
  -> 중복 값 없이 다른 노드를 참조

이 구조 덕분에 BPB는 사실상 “TOML/JSON 스타일 설정 트리”를 1차원 배열로 인코딩한 셈이 된다.

### 7.3 BLOB 영역

문자열로 표현하기 미묘한 데이터(TOML 원문, DTS 바이너리, 서명, 인증서 등)는
BLOB 영역에 저장하며, 각 BLOB은 다음 헤더를 가진다:

* `leyn_bpb_cfg_blob_header`:

  * `kind`:

    * 상위 4비트 도메인(`GENERIC`, `DTS`, `IPC`, `MODULE`)
    * 하위 4비트 서브타입/순서 (`MODULE_POLICY`, `MODULE_CAPS`, `MODULE_SIGN` 등)
  * `version`: 해당 BLOB 포맷 버전
  * `flags`: 압축/암호화 등
  * `payload_size`: 페이로드 크기
  * 뒤이어 `payload[payload_size]`
  * 마지막에 `tail_magic = 0xA5`로 end 검증

CONFIG_GRAPH 노드의 `value_type = BLOB`일 경우:

* `value.u64_value`는 **blob_area 내 오프셋**을 가리킨다.
* 커널은 BLOB 헤더를 읽고:

  * kind 도메인/서브타입 확인
  * version/flags 검증
  * tail_magic 검증
  * payload를 해당 도메인 로직으로 파싱

이렇게 해서 하나의 BPB 안에:

* TOML 설정 파일 원문
* DTS/DTB 바이너리
* 모듈 정책/서명/캡어빌리티 정보
  를 전부 담을 수 있다.

---

## 8. 모듈 시스템 & 안전 모델

모듈 관련 정보는 주로 `MODULE` 도메인의 BLOB으로 전달된다.

### 8.1 모듈 정책 / 서명 / 캡어빌리티

* `LEYN_BPB_CFG_BLOB_KIND_MODULE_POLICY`

  * 모듈 로딩 순서, 필수/옵션 여부, 실패 시 동작(reboot, degrade 등)
* `LEYN_BPB_CFG_BLOB_KIND_MODULE_SIGN`

  * 모듈 바이너리 해시, 서명 데이터, 인증서 체인 등
* `LEYN_BPB_CFG_BLOB_KIND_MODULE_CAPS`

  * `leyn_bpb_module_caps` 구조체 payload:

```c
struct leyn_bpb_module_caps {
    uint32_t api_version;
    uint32_t flags;           /* leyn_bpb_module_cap_flags */
    uint64_t allowed_irq_mask;
    uint64_t io_port_base;
    uint64_t io_port_count;
    uint64_t mmio_base;
    uint64_t mmio_size;
};
```

캡어빌리티 플래그 예:

* `ALLOW_IO_PORT`, `ALLOW_MMIO`, `ALLOW_DMA`, `ALLOW_IRQ`
* `ALLOW_PRIV` : 특권 명령/커널 API
* `REQUIRE_SIGNED` : 서명 필수
* `HARD_RT` : 하드 실시간 제약 하에서 동작

### 8.2 CONFIG_GRAPH와의 연결

CONFIG_GRAPH 노드 예시(개념):

* 키: `"modules.uart0.path"`

  * `STRING` -> `/boot/drv/uart0.bin`
* 키: `"modules.uart0.caps_blob"`

  * `BLOB` -> MODULE_CAPS blob offset
* 키: `"modules.uart0.policy_blob"`

  * `BLOB` -> MODULE_POLICY blob offset
* 키: `"modules.uart0.enabled"`

  * `BOOL` -> 1

노드 플래그:

* 모듈 로딩 정책은 대부분 `READONLY | SECURE | HOT`
* runtime 튜닝 파라미터(예: scheduler quantum)는 `RUNTIME | HOT`
* 내부 전용 키는 `INTERNAL`

이렇게 하면 Leyn 커널은:

1. CONFIG_GRAPH에서 모듈 리스트를 읽고
2. 각 모듈에 연결된 BLOB들을 통해 정책/권한/서명을 파악한 뒤
3. 초기화 시점에 모듈을 **정확한 권한과 제약 조건 하에서 로딩**할 수 있다.

일반적인 “/etc/modules.conf” 수준을 넘어,
**커널 모듈을 거의 capability-based security 모델로 다룰 수 있는 기반**을 제공한다.

---

## 9. Multiboot2와의 호환성

Multiboot2 boot information structure는 **8바이트 정렬된 태그들의 리스트** 구성이며,
각 태그는 type/size 헤더 뒤에 payload가 붙는 구조를 가진다.

Leyn BPB는 “태그 리스트” 대신 “섹션 테이블 + payload”를 사용하지만,
실제 전달하는 정보의 의미는 다음과 같이 1:1로 매핑된다.

| Multiboot2 태그                   | Leyn BPB 섹션/구조체                          |
| ------------------------------- | ---------------------------------------------- |
| Basic meminfo (mem_lower/upper) | `LEYN_BPB_SEC_BASIC_MEMINFO` + `basic_meminfo` |
| Memory map                      | `LEYN_BPB_SEC_MEMORY_MAP` + `mmap_entry[]`     |
| BIOS boot device                | `LEYN_BPB_SEC_BOOT_DEVICE_BIOS`                |
| Modules                         | `LEYN_BPB_SEC_BOOT_MODULES` + `boot_module[]`  |
| Boot command line               | `LEYN_BPB_SEC_BOOT_CMDLINE`                    |
| Bootloader name                 | `LEYN_BPB_SEC_BOOTLOADER_NAME`                 |
| VBE info                        | `LEYN_BPB_SEC_VBE_INFO`                        |
| Framebuffer info                | `LEYN_BPB_SEC_FRAMEBUFFER_INFO` + `fb_info`    |
| ELF sections                    | `LEYN_BPB_SEC_ELF_SYMBOLS`                     |
| APM table                       | `LEYN_BPB_SEC_APM_TABLE`                       |
| ACPI RSDP v1/v2                 | `LEYN_BPB_SEC_ACPI_RSDP_V1 / _V2`              |
| SMBIOS                          | `LEYN_BPB_SEC_SMBIOS`                          |
| Network info                    | `LEYN_BPB_SEC_NETWORK_DHCP`                    |
| EFI system table                | `LEYN_BPB_SEC_EFI_SYSTEM_TABLE`                |
| EFI memory map                  | `LEYN_BPB_SEC_EFI_MEMORY_MAP`                  |
| EFI image handle                | `LEYN_BPB_SEC_EFI_IMAGE_HANDLE`                |
| Image load phys addr            | `LEYN_BPB_SEC_IMAGE_LOAD_BASE`                 |

따라서:

* 나중에 Leyn을 GRUB/Multiboot2로도 부팅시키고 싶다면

  * GRUB이 제공하는 Multiboot2 정보 구조를 파싱해서
  * Ribon이 “Multiboot2 -> Leyn BPB” 변환기를 작성할 수 있다.
* 반대로, 지금 Ribon + BPB 구조에 익숙해지면 **Multiboot2를 직접 구현하지 않아도**
  Multiboot2가 주는 정보와 동일한 수준의 환경 서술을 이미 갖고 있는 셈이다.

---

## 10. UEFI vs Non-UEFI 정보 분리

Leyn BPB는 “정보의 출처(source)”를 **두 레벨에서 구분**한다.

1. **전역 헤더 플래그**

   * `LEYN_BPB_FLAG_EFI_BOOT` / `LEYN_BPB_FLAG_LEGACY_BOOT`
   * `LEYN_BPB_FLAG_HAS_EFI_SYSTEM_TABLE`
   * `LEYN_BPB_FLAG_HAS_EFI_IMAGE_HANDLE`
   * `LEYN_BPB_FLAG_EFI_BOOT_SERVICES_ON` 등
2. **섹션 플래그**

   * `LEYN_BPB_SEC_FLAG_SRC_EFI` : UEFI에서 직접 얻은 정보
   * `LEYN_BPB_SEC_FLAG_SRC_BIOS`: 레거시 BIOS에서 얻은 정보
   * `LEYN_BPB_SEC_FLAG_SRC_FDT` : 디바이스 트리(FDT/DTB)에서 파생된 정보

예시:

* ACPI RSDP가 UEFI System Table에서 얻어진 경우:

  * `LEYN_BPB_SEC_ACPI_RSDP_V2`에
  * `SEC_FLAG_SRC_EFI` 세트
* ACPI RSDP를 BIOS 스캔으로 찾은 경우:

  * `SEC_FLAG_SRC_BIOS`
* device tree 기반 ARM 보드에서 DT로부터 메모리/장치 정보를 파생했다면:

  * `LEYN_BPB_SEC_DEVICE_TREE` + `SEC_FLAG_SRC_FDT`

커널 입장에서는:

* “UEFI가 있는 x86_64 PC냐, BIOS-only냐, pure FDT ARM 보드냐?”
  를 전역 플래그로 대략 판단하고,
* 섹션 단위의 SRC 플래그로 각 데이터의 신뢰·우선 순위를 조정할 수 있다.

---

## 11. 메모리 배치 및 성능 고려

성능을 염두에 두고 BPB를 읽을 때는 다음 가이드를 따르면 된다.

1. **핫패스 섹션**

   * `CORE_INFO`, `MEMORY_MAP`, `BASIC_MEMINFO`, `FRAMEBUFFER_INFO`,
     `CONFIG_GRAPH`의 일부(HOT 노드) 등은
   * `SEC_FLAG_CORE | SEC_FLAG_HOT | SEC_FLAG_EARLY_MAP` 등을 세트하여
   * BPB의 앞부분에 위치시키고,
     초기 커널 코드에서 한 번에 prefetch/파싱한다.

2. **콜드 섹션**

   * DEBUG_LOG, SMBIOS, NETWORK_DHCP, FULL CONFIG_GRAPH 등은
   * `SEC_FLAG_OPTIONAL | SEC_FLAG_COLD`로 마킹
   * 필요할 때만 lazy하게 파싱/디코딩

3. **압축(LZ4) 섹션**

   * 큰 TOML/BLOB/로그/디바이스 트리 등은
   * `SEC_FLAG_COMP_LZ4` 플래그와 `uncompressed_size`를 사용해 압축
   * 커널은 BPB를 읽은 뒤,
     자신이 관리하는 메모리 위로 “디코딩된 섹션”을 재배치할 수 있다.

4. **CONFIG_GRAPH HOT 노드**

   * `LEYN_BPB_CFG_F_HOT`가 세트된 노드는
     커널이 설정 파서를 초기화할 때 우선적으로 인덱싱해두면,
   * IPC/스케줄러 튜닝 파라미터를 빠르게 읽을 수 있다.

이런 전략을 쓰면, **부팅 초기 수천 사이클 안에 필요한 정보만 CPU cache에 들어오게 하는 설계**가 가능하다.

---

## 12. 실제 헤더(leyn_bpb.h)

```c
#ifndef LEYN_BPB_H
#define LEYN_BPB_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @file leyn_bpb.h
 *  @brief Leyn 하이브리드 커널을 위한 Boot Profile Binary(BPB) 포맷 정의
 *
 * 이 헤더는 Ribon 부트로더와 Leyn 커널 사이에서 사용되는
 * 부트 인자 전달 포맷(BPB)을 정의한다.
 * - 섹션 기반 모듈 구조
 * - LZ4 압축 지원
 * - TOML/DTS/모듈 정책 등을 담는 설정 그래프(CONFIG_GRAPH) 지원
 * - UEFI/BIOS/디바이스 트리 기원 정보를 구분하는 플래그
 * - Multiboot2가 제공하는 모든 정보에 대응 가능한 섹션 집합
 * - 메모리 부족 환경에서 코어 섹션만 사용하는 최소 부팅 모드 지원
 */

/* -------------------------------------------------------------------------- */
/* 매직/버전/아키텍처                                                         */
/* -------------------------------------------------------------------------- */

/** @brief BPB 매직 값 ('LBPB') */
#define LEYN_BPB_MAGIC 0x4C425042u  /* 'L' 'B' 'P' 'B' */

/** @brief BPB 주요 버전 */
#define LEYN_BPB_VERSION_MAJOR 1u   /* BPB 메이저 버전 */

/** @brief BPB 부 버전 */
#define LEYN_BPB_VERSION_MINOR 0u   /* BPB 마이너 버전 */

/** @brief BPB 대상 아키텍처 */
enum leyn_bpb_arch {
    LEYN_BPB_ARCH_UNKNOWN = 0u,  /* 알 수 없음/기본값 */
    LEYN_BPB_ARCH_X86_64  = 1u,  /* x86_64 아키텍처 */
    LEYN_BPB_ARCH_AARCH64 = 2u,  /* AArch64(ARM64) 아키텍처 */
    LEYN_BPB_ARCH_RISCV64 = 3u,  /* RISC-V 64비트 아키텍처 */
    /* 향후 다른 아키텍처 추가 가능 */
};

/* -------------------------------------------------------------------------- */
/* BPB 전체 플래그                                                            */
/* -------------------------------------------------------------------------- */

/** @brief BPB 헤더 플래그 */
enum leyn_bpb_flags {
    LEYN_BPB_FLAG_LITTLE_ENDIAN        = (1u << 0),  /* BPB가 리틀엔디안 포맷으로 인코딩됨 */
    LEYN_BPB_FLAG_64BIT_ADDR           = (1u << 1),  /* 64비트 주소 공간을 사용하는 커널용 BPB */
    LEYN_BPB_FLAG_CORE_ONLY            = (1u << 2),  /* 부트로더가 코어 섹션만 포함시킨 최소 구성 */
    LEYN_BPB_FLAG_HAS_CHECKSUM         = (1u << 3),  /* checksum 필드가 유효함 */

    LEYN_BPB_FLAG_REQUIRE_MEMMAP       = (1u << 4),  /* 메모리 맵 섹션이 필수 */
    LEYN_BPB_FLAG_REQUIRE_DEVICE_TREE  = (1u << 5),  /* 디바이스 트리 섹션이 필수 */
    LEYN_BPB_FLAG_REQUIRE_CONFIG_GRAPH = (1u << 6),  /* CONFIG_GRAPH 섹션이 필수 */

    LEYN_BPB_FLAG_EFI_BOOT             = (1u << 7),  /* UEFI 부팅 경로에서 생성된 BPB */
    LEYN_BPB_FLAG_LEGACY_BOOT          = (1u << 8),  /* 레거시 BIOS/기타 부트 경로 */

    LEYN_BPB_FLAG_ENTRY_PHYS_VALID     = (1u << 9),  /* kernel_entry_vaddr가 물리 주소로 유효 */
    LEYN_BPB_FLAG_ENTRY_VIRT_VALID     = (1u << 10), /* kernel_entry_vaddr가 가상 주소로 유효 */

    LEYN_BPB_FLAG_HAS_ACPI             = (1u << 11), /* ACPI RSDP 관련 섹션 존재 */
    LEYN_BPB_FLAG_HAS_SMBIOS           = (1u << 12), /* SMBIOS 섹션 존재 */
    LEYN_BPB_FLAG_HAS_EFI_SYSTEM_TABLE = (1u << 13), /* EFI 시스템 테이블 포인터 섹션 존재 */
    LEYN_BPB_FLAG_HAS_EFI_IMAGE_HANDLE = (1u << 14), /* EFI 이미지 핸들 포인터 섹션 존재 */
    LEYN_BPB_FLAG_EFI_BOOT_SERVICES_ON = (1u << 15), /* ExitBootServices() 호출되지 않음 */
    LEYN_BPB_FLAG_HAS_NETWORK          = (1u << 16), /* 네트워크(DHCP 등) 부트 정보 존재 */
    LEYN_BPB_FLAG_HAS_FB_INFO          = (1u << 17), /* FRAMEBUFFER_INFO 섹션 존재 */
    LEYN_BPB_FLAG_HAS_MODULES          = (1u << 18), /* BOOT_MODULES 섹션 존재 */
};

/* -------------------------------------------------------------------------- */
/* 섹션 타입                                                                 */
/* -------------------------------------------------------------------------- */

/** @brief BPB 섹션 타입 */
enum leyn_bpb_section_type {
    LEYN_BPB_SEC_CORE_INFO        = 0x0001u, /* 커널 코어 기동 정보(엔트리, 모드 등) */
    LEYN_BPB_SEC_MEMORY_MAP       = 0x0002u, /* 정규화된 물리 메모리 맵(e820 스타일) */
    LEYN_BPB_SEC_KERNEL_IMAGE     = 0x0003u, /* 커널 이미지에 대한 메타 데이터 */
    LEYN_BPB_SEC_INITRD           = 0x0004u, /* NuttX 루트 FS 또는 initrd 이미지 */
    LEYN_BPB_SEC_DEVICE_TREE      = 0x0005u, /* DTB 또는 유사 디바이스 트리 데이터 */
    LEYN_BPB_SEC_CONFIG_GRAPH     = 0x0006u, /* TOML/DTS/모듈 정책을 담는 설정 그래프 */
    LEYN_BPB_SEC_IPC_PROFILE      = 0x0007u, /* IPC 엔드포인트 그래프 및 성능 프로필 */
    LEYN_BPB_SEC_SCHED_PROFILE    = 0x0008u, /* 스케줄러/실시간 프로필 정보 */
    LEYN_BPB_SEC_MODULE_POLICY    = 0x0009u, /* 커널 모듈/드라이버 로딩 정책(옵션, CONFIG_GRAPH 캐시) */
    LEYN_BPB_SEC_DEBUG_LOG        = 0x000Au, /* 초기 부트 로그/진단 정보 */
    LEYN_BPB_SEC_FRAMEBUFFER_INFO = 0x000Bu, /* 통합 framebuffer/그래픽 모드 정보 */

    /* Multiboot2 호환/플랫폼 정보 섹션 */
    LEYN_BPB_SEC_BASIC_MEMINFO    = 0x0010u, /* 저/고 메모리 크기 정보 (mem_lower/mem_upper) */
    LEYN_BPB_SEC_BOOT_DEVICE_BIOS = 0x0011u, /* BIOS 부트 디스크/파티션 정보 */
    LEYN_BPB_SEC_BOOT_CMDLINE     = 0x0012u, /* 커널 부트 커맨드라인 (UTF-8, \0 종료 문자열) */
    LEYN_BPB_SEC_BOOTLOADER_NAME  = 0x0013u, /* 부트로더 이름 (UTF-8, \0 종료 문자열) */
    LEYN_BPB_SEC_ELF_SYMBOLS      = 0x0014u, /* ELF 섹션 헤더/심볼 테이블 정보 */
    LEYN_BPB_SEC_APM_TABLE        = 0x0015u, /* APM BIOS 테이블 정보 */
    LEYN_BPB_SEC_VBE_INFO         = 0x0016u, /* VESA VBE 컨트롤/모드 정보 (legacy BIOS) */
    LEYN_BPB_SEC_ACPI_RSDP_V1     = 0x0017u, /* ACPI 1.0 RSDP 원본 복사본 */
    LEYN_BPB_SEC_ACPI_RSDP_V2     = 0x0018u, /* ACPI 2.0+ RSDP(XSDP) 원본 복사본 */
    LEYN_BPB_SEC_SMBIOS           = 0x0019u, /* SMBIOS 테이블 복사본 */
    LEYN_BPB_SEC_NETWORK_DHCP     = 0x001Au, /* 네트워크(DHCP ACK) 부트 정보 */
    LEYN_BPB_SEC_EFI_SYSTEM_TABLE = 0x001Bu, /* EFI 시스템 테이블 포인터 */
    LEYN_BPB_SEC_EFI_MEMORY_MAP   = 0x001Cu, /* EFI 메모리 맵 원본 복사본 */
    LEYN_BPB_SEC_EFI_IMAGE_HANDLE = 0x001Du, /* EFI 이미지 핸들 포인터 */
    LEYN_BPB_SEC_IMAGE_LOAD_BASE  = 0x001Eu, /* 커널 이미지 로드 베이스 물리 주소 */
    LEYN_BPB_SEC_BOOT_MODULES     = 0x001Fu, /* 부트 모듈 목록(여러 initrd/모듈 지원) */

    /* 그래픽/콘솔 관련 세부 정보 */
    LEYN_BPB_SEC_UEFI_GOP_MODE    = 0x0020u, /* UEFI GOP 모드 상세 정보 */
    LEYN_BPB_SEC_VGA_TEXT_MODE    = 0x0021u, /* VGA 텍스트 모드 정보 (cols/rows 등) */
    LEYN_BPB_SEC_EDID             = 0x0022u, /* EDID/DisplayID 등 디스플레이 식별 정보 */

    LEYN_BPB_SEC_VENDOR_START     = 0x8000u, /* Leyn 전용/실험적 섹션 시작 범위 */
};

/* -------------------------------------------------------------------------- */
/* 섹션 플래그                                                               */
/* -------------------------------------------------------------------------- */

/** @brief BPB 섹션 플래그 */
enum leyn_bpb_section_flags {
    LEYN_BPB_SEC_FLAG_CORE        = (1u << 0),  /* 코어 부팅에 필수인 섹션 */
    LEYN_BPB_SEC_FLAG_OPTIONAL    = (1u << 1),  /* 없어도 부팅은 가능한 선택 섹션 */

    /* 압축 플래그 (현재는 LZ4만 정의, 향후 확장 가능) */
    LEYN_BPB_SEC_FLAG_COMP_LZ4    = (1u << 2),  /* 섹션 페이로드가 LZ4로 압축됨 */

    LEYN_BPB_SEC_FLAG_EARLY_MAP   = (1u << 3),  /* 커널 매우 초기 단계에서 매핑이 필요함 */
    LEYN_BPB_SEC_FLAG_RUNTIME     = (1u << 4),  /* 런타임 동안 계속 유지해야 하는 데이터 */
    LEYN_BPB_SEC_FLAG_DISCARDABLE = (1u << 5),  /* 초기화 이후 해제해도 되는 섹션 */
    LEYN_BPB_SEC_FLAG_PERSIST     = (1u << 6),  /* 절대 해제하면 안 되는 섹션 */
    LEYN_BPB_SEC_FLAG_HW_RELATED  = (1u << 7),  /* 하드웨어/플랫폼 관련 데이터 */

    /* 원천(출처) 플래그: UEFI/BIOS/DT 등 */
    LEYN_BPB_SEC_FLAG_SRC_EFI     = (1u << 8),  /* UEFI에서 직접 얻은 정보 */
    LEYN_BPB_SEC_FLAG_SRC_BIOS    = (1u << 9),  /* BIOS/레거시 펌웨어에서 얻은 정보 */
    LEYN_BPB_SEC_FLAG_SRC_FDT     = (1u << 10), /* 디바이스 트리(FDT)에서 얻은 정보 */

    /* 캐시/레이아웃 힌트 */
    LEYN_BPB_SEC_FLAG_HOT         = (1u << 11), /* 부트 초기/핫패스에서 자주 접근 */
    LEYN_BPB_SEC_FLAG_COLD        = (1u << 12), /* 진단/디버그 등 거의 접근하지 않음 */
};

/* -------------------------------------------------------------------------- */
/* 정렬/레이아웃 가이드 (권장값, 강제 아님)                                   */
/* -------------------------------------------------------------------------- */

/** @brief BPB 전체 구조체 정렬에 대한 권장 캐시 라인 크기(바이트) */
#define LEYN_BPB_RECOMMENDED_ALIGN     64u

/** @brief 섹션 테이블의 권장 정렬 단위(바이트) */
#define LEYN_BPB_SECTION_TABLE_ALIGN   64u

/** @brief 섹션 페이로드의 기본 정렬 단위(바이트) */
#define LEYN_BPB_SECTION_PAYLOAD_ALIGN 64u

/* -------------------------------------------------------------------------- */
/* BPB 헤더                                                                  */
/* -------------------------------------------------------------------------- */

/** @brief Leyn Boot Profile Binary(BPB) 공통 헤더 */
struct leyn_bpb_header {
    /** @brief 매직 값 (LEYN_BPB_MAGIC) */
    uint32_t magic;

    /** @brief BPB 메이저 버전 */
    uint16_t version_major;

    /** @brief BPB 마이너 버전 */
    uint16_t version_minor;

    /** @brief 대상 아키텍처 (leyn_bpb_arch) */
    uint16_t arch;

    /** @brief 아키텍처 관련 예약 필드 (정렬/확장용) */
    uint16_t arch_reserved;

    /** @brief 이 헤더 구조체의 크기 (바이트 단위) */
    uint32_t header_size;

    /** @brief BPB 전체 크기 (헤더 + 섹션 테이블 + 모든 페이로드, 바이트 단위) */
    uint32_t total_size;

    /** @brief 섹션 개수 */
    uint32_t section_count;

    /** @brief BPB 플래그 (leyn_bpb_flags의 비트 OR) */
    uint32_t flags;

    /**
     * @brief BPB 전체에 대한 체크섬 (옵션)
     *
     * LEYN_BPB_FLAG_HAS_CHECKSUM 플래그가 설정된 경우 유효하다.
     * 체크섬 알고리즘은 구현에서 별도로 정의한다(예: CRC32, xxHash 등).
     */
    uint64_t checksum;

    /**
     * @brief 커널 엔트리 주소
     *
     * flags 필드의 LEYN_BPB_FLAG_ENTRY_PHYS_VALID / ENTRY_VIRT_VALID
     * 비트를 통해 물리/가상 주소 여부를 구분한다.
     */
    uint64_t kernel_entry_vaddr;

    /** @brief 예약 필드 (미래 확장용) */
    uint64_t reserved[2];
};

/* -------------------------------------------------------------------------- */
/* 섹션 테이블 엔트리                                                        */
/* -------------------------------------------------------------------------- */

/** @brief BPB 섹션 테이블 엔트리 */
struct leyn_bpb_section {
    /** @brief 섹션 타입 (leyn_bpb_section_type) */
    uint32_t type;

    /** @brief 섹션 플래그 (leyn_bpb_section_flags의 비트 OR) */
    uint16_t flags;

    /** @brief 섹션 페이로드 정렬 요구사항 (바이트 단위, 2의 제곱 권장) */
    uint16_t alignment;

    /** @brief 예약 필드 (구조체 정렬 및 확장용) */
    uint32_t reserved;

    /**
     * @brief BPB 시작 주소로부터의 섹션 페이로드 오프셋 (바이트 단위)
     *
     * leyn_bpb_header의 시작 주소를 기준으로 한 상대 오프셋이다.
     */
    uint64_t payload_offset;

    /**
     * @brief 섹션 페이로드의 실제 크기 (바이트 단위)
     *
     * 압축 플래그가 설정된 경우 "압축된 크기"를 의미한다.
     */
    uint64_t payload_size;

    /**
     * @brief 압축 해제 후 섹션 페이로드의 크기 (바이트 단위)
     *
     * 압축 플래그가 설정되지 않은 경우,
     * payload_size와 동일한 값을 넣는 것이 권장된다.
     */
    uint64_t uncompressed_size;

    /**
     * @brief 섹션 별 추가 정보 (옵션 필드)
     *
     * 예: CONFIG_GRAPH 섹션의 경우 노드 개수, 문자열 테이블 오프셋 등
     * 섹션 타입별 의미를 구현에서 정의할 수 있다.
     */
    uint64_t aux_data;
};

/* -------------------------------------------------------------------------- */
/* 메모리 관련 페이로드 구조체                                               */
/* -------------------------------------------------------------------------- */

/** @brief 기본 메모리 정보 (Multiboot2 mem_lower/mem_upper 대응) */
struct leyn_bpb_basic_meminfo {
    uint32_t mem_lower_kb;  /* 0~640KB 영역 크기 (KB 단위) */
    uint32_t mem_upper_kb;  /* 1MB 이상 첫 홀까지의 크기 (KB 단위) */
};

/** @brief 정규화된 메모리 맵 엔트리 (e820 스타일) */
struct leyn_bpb_mmap_entry {
    uint64_t base_addr;  /* 물리 베이스 주소 */
    uint64_t length;     /* 영역 크기(바이트) */
    uint32_t type;       /* 1: 사용 가능 RAM, 3: ACPI, 4: NVS, 5: 불량 RAM 등 */
    uint32_t flags;      /* 향후 확장을 위한 플래그(현재 0으로 채움) */
};

/* -------------------------------------------------------------------------- */
/* 부트 디바이스/모듈/ELF 정보                                               */
/* -------------------------------------------------------------------------- */

/** @brief BIOS 부트 디바이스 정보 (Multiboot2 BIOS Boot device 대응) */
struct leyn_bpb_boot_device_bios {
    uint32_t biosdev;      /* INT 0x13 기준 BIOS 드라이브 번호 (0x80 등) */
    uint32_t partition;    /* 최상위 파티션 번호, 없으면 0xFFFFFFFF */
    uint32_t sub_partition;/* 하위 파티션 번호, 사용하지 않으면 0xFFFFFFFF */
};

/** @brief 부트 모듈(예: 여러 initrd)의 메타데이터 */
struct leyn_bpb_boot_module {
    uint64_t phys_start;   /* 모듈 시작 물리 주소 */
    uint64_t phys_end;     /* 모듈 끝 물리 주소(마지막 바이트+1) */
    uint32_t string_offset;/* 모듈을 설명하는 문자열 오프셋(섹션 내 문자열 테이블 기준) */
    uint32_t flags;        /* 모듈 개별 플래그(정책/우선순위 등, 구현 정의) */
};

/** @brief ELF 섹션/심볼 테이블 정보 (Multiboot2 ELF-Symbols 대응) */
struct leyn_bpb_elf_symbols {
    uint16_t num;       /* 섹션 헤더 개수 */
    uint16_t entsize;   /* 각 섹션 헤더 엔트리 크기 */
    uint16_t shndx;     /* 섹션 이름 문자열 테이블 인덱스 */
    uint16_t reserved;  /* 정렬용 */
    uint64_t shdr_addr; /* 메모리 상 섹션 헤더 테이블 물리/가상 주소 */
};

/* -------------------------------------------------------------------------- */
/* ACPI / SMBIOS / EFI / 네트워크                                            */
/* -------------------------------------------------------------------------- */

/**
 * @brief EFI 시스템 테이블 포인터
 *
 * UEFI 플랫폼에서는 EFI 시스템 테이블의 물리 주소를 그대로 전달한다.
 * 32비트/64비트 플랫폼 모두 64비트 필드로 표현한다.
 */
struct leyn_bpb_efi_system_table_ptr {
    uint64_t system_table_phys;  /* EFI_SYSTEM_TABLE* 의 물리 주소 */
};

/** @brief EFI 이미지 핸들 포인터 (보통 부트로더 이미지 핸들) */
struct leyn_bpb_efi_image_handle_ptr {
    uint64_t image_handle;       /* EFI_HANDLE 의 값 */
};

/**
 * @brief EFI 메모리 맵 메타데이터
 *
 * 페이로드에는 EFI 메모리 맵 엔트리 배열이 descriptor_size 단위로
 * 연속으로 저장된다(EFI 사양 참고).
 */
struct leyn_bpb_efi_mmap_meta {
    uint32_t descriptor_size;    /* EFI 메모리 디스크립터 크기 */
    uint32_t descriptor_version; /* EFI 메모리 디스크립터 버전 */
};

/**
 * @brief 네트워크 부트 정보 (DHCP ACK 전체를 원본 그대로 저장)
 *
 * 페이로드는 DHCP 패킷 전체를 그대로 포함한다.
 */
struct leyn_bpb_net_dhcp_meta {
    uint32_t if_index;           /* 네트워크 인터페이스 인덱스(부트로더 정의) */
    uint32_t reserved;           /* 정렬/확장용 */
    /* 뒤이어 실제 DHCP ACK 패킷 바이트 배열이 온다. */
};

/* ACPI RSDP, SMBIOS, APM 등은 원본 테이블을 그대로 페이로드에 저장하므로
 * 별도 구조체를 강제하지 않는다. 커널은 해당 스펙을 따라 직접 파싱하면 된다. */

/* -------------------------------------------------------------------------- */
/* 그래픽/콘솔: 통합 Framebuffer + UEFI GOP + VESA/VGA                       */
/* -------------------------------------------------------------------------- */

/** @brief Leyn 표준 framebuffer 타입 */
enum leyn_bpb_fb_type {
    LEYN_BPB_FB_TYPE_INDEXED = 0u, /* 인덱스 팔레트 방식 */
    LEYN_BPB_FB_TYPE_RGB     = 1u, /* Direct RGB (마스크 기반) */
    LEYN_BPB_FB_TYPE_TEXT    = 2u, /* 텍스트 모드 (VGA/EGA 등) */
};

/** @brief framebuffer 백엔드 종류(정보 출처) */
enum leyn_bpb_fb_backend {
    LEYN_BPB_FB_BACKEND_UNKNOWN  = 0u, /* 알 수 없음/혼합 */
    LEYN_BPB_FB_BACKEND_UEFI_GOP = 1u, /* UEFI Graphics Output Protocol 기반 */
    LEYN_BPB_FB_BACKEND_VBE      = 2u, /* VESA VBE 모드 기반 */
    LEYN_BPB_FB_BACKEND_VGA_TEXT = 3u, /* VGA 텍스트 모드 */
};

/** @brief Direct RGB framebuffer의 색 마스크 정보 */
struct leyn_bpb_fb_rgb_info {
    uint8_t red_position;      /* R 비트 필드 시작 위치 */
    uint8_t red_mask_size;     /* R 비트 필드 길이 */
    uint8_t green_position;    /* G 비트 필드 시작 위치 */
    uint8_t green_mask_size;   /* G 비트 필드 길이 */
    uint8_t blue_position;     /* B 비트 필드 시작 위치 */
    uint8_t blue_mask_size;    /* B 비트 필드 길이 */
    uint8_t reserved[2];       /* 정렬용 */
};

/**
 * @brief 통합 framebuffer 정보 (Multiboot2 Framebuffer tag + 백엔드 구분)
 *
 * 이 구조체는 실제 프레임버퍼의 레이아웃을 서술하며,
 * UEFI GOP/VBE/VGA 텍스트 모드 모두를 하나의 형태로 표현한다.
 */
struct leyn_bpb_fb_info {
    uint64_t framebuffer_addr;   /* 프레임버퍼 물리 주소 */
    uint32_t framebuffer_pitch;  /* 한 스캔라인의 바이트 수 */
    uint32_t framebuffer_width;  /* 가로 크기 (픽셀 또는 문자) */
    uint32_t framebuffer_height; /* 세로 크기 (픽셀 또는 문자) */

    uint8_t  framebuffer_bpp;    /* 픽셀 당 비트 수, 텍스트 모드일 때는 16 */
    uint8_t  framebuffer_type;   /* leyn_bpb_fb_type */
    uint8_t  backend;            /* leyn_bpb_fb_backend */
    uint8_t  reserved0;          /* 정렬용 */

    union {
        struct {
            uint32_t num_colors;      /* 팔레트 엔트리 개수 */
            uint64_t palette_offset;  /* 팔레트 데이터 오프셋(섹션 페이로드 기준) */
        } indexed;                    /* 인덱스 컬러 모드 */

        struct leyn_bpb_fb_rgb_info rgb; /* Direct RGB 모드 */
    } color;
};

/**
 * @brief UEFI GOP 모드 정보(필요 시 원본 보존용)
 *
 * 대부분의 커널 코드는 leyn_bpb_fb_info 만으로도 충분하지만,
 * GOP 고유의 정보가 필요할 경우 이 구조체를 함께 사용한다.
 */
struct leyn_bpb_uefi_gop_mode {
    uint32_t version;             /* EFI_GRAPHICS_OUTPUT_MODE_INFORMATION::Version */
    uint32_t horizontal_resolution;
    uint32_t vertical_resolution;
    uint32_t pixels_per_scanline;

    uint32_t pixel_format;        /* EFI_GRAPHICS_PIXEL_FORMAT 상응 값 (RGB/BGR/BitMask 등) */
    uint32_t reserved;            /* 정렬용 */

    uint64_t framebuffer_base;    /* FrameBufferBase (물리 주소) */
    uint64_t framebuffer_size;    /* FrameBufferSize (바이트) */

    uint64_t mode_info_phys;      /* 원본 GOP MODE_INFORMATION 복사본의 물리 주소(옵션) */
};

/**
 * @brief VESA VBE 정보 (Multiboot2 VBE info 대응, legacy BIOS 전용)
 *
 * VBE Function 00h/01h 호출 결과를 그대로 저장한다.
 */
struct leyn_bpb_vbe_info {
    uint16_t vbe_mode;
    uint16_t vbe_interface_seg;
    uint16_t vbe_interface_off;
    uint16_t vbe_interface_len;
    uint8_t  vbe_control_info[512];
    uint8_t  vbe_mode_info[256];
};

/**
 * @brief VGA 텍스트 모드 정보
 *
 * UEFI 시스템에서도 펌웨어가 VGA 텍스트 모드를 제공할 수 있으므로
 * 레거시와 동일한 구조로 기술한다.
 */
struct leyn_bpb_vga_text_mode {
    uint16_t columns;          /* 문자 단위 가로 폭 */
    uint16_t rows;             /* 문자 단위 세로 높이 */
    uint16_t char_height;      /* 한 문자의 세로 픽셀 수(폰트 높이) */
    uint16_t reserved;         /* 정렬용 */
    uint64_t text_buffer_phys; /* 텍스트 버퍼 물리 주소(보통 0xB8000) */
};

/* -------------------------------------------------------------------------- */
/* CONFIG_GRAPH 섹션: 설정 노드/메타데이터                                   */
/* -------------------------------------------------------------------------- */

/** @brief CONFIG_GRAPH 섹션 내 설정 값 타입 */
enum leyn_bpb_cfg_value_type {
    LEYN_BPB_CFG_NONE    = 0u, /* 값 없음/사용되지 않음 */
    LEYN_BPB_CFG_INT     = 1u, /* 정수 값(int64_t) */
    LEYN_BPB_CFG_UINT    = 2u, /* 부호 없는 정수(uint64_t) */
    LEYN_BPB_CFG_BOOL    = 3u, /* 불리언 (0 또는 1) */
    LEYN_BPB_CFG_FLOAT   = 4u, /* 실수(double) */
    LEYN_BPB_CFG_STRING  = 5u, /* 문자열 (문자열 테이블 오프셋) */
    LEYN_BPB_CFG_BLOB    = 6u, /* 임의 바이너리 데이터(BLOB 영역 오프셋) */
    LEYN_BPB_CFG_OBJECT  = 7u, /* 하위 노드 집합을 가리키는 오브젝트 */
    LEYN_BPB_CFG_ARRAY   = 8u, /* 연속 노드 인덱스 배열(구현 정의) */
    LEYN_BPB_CFG_REF     = 9u, /* 다른 노드를 가리키는 참조(노드 인덱스) */
};

/** @brief CONFIG_GRAPH 노드 플래그 */
enum leyn_bpb_cfg_node_flags {
    LEYN_BPB_CFG_F_READONLY  = (1u << 0), /* 런타임에서 수정 불가한 설정 */
    LEYN_BPB_CFG_F_SECURE    = (1u << 1), /* 서명 검증 등 보안 경로를 통해서만 변경 가능 */
    LEYN_BPB_CFG_F_RUNTIME   = (1u << 2), /* 런타임에서 변경 가능(예: 튜닝 파라미터) */
    LEYN_BPB_CFG_F_DEPRECATED= (1u << 3), /* 더 이상 사용되지 않는 설정 */
    LEYN_BPB_CFG_F_INTERNAL  = (1u << 4), /* 사용자 노출 대상이 아닌 내부 설정 */
    LEYN_BPB_CFG_F_HOT       = (1u << 5), /* 부트/핫패스에서 자주 읽는 노드 */
};

/** @brief CONFIG_GRAPH 섹션 내 설정 노드 */
struct leyn_bpb_cfg_node {
    /**
     * @brief 다음 노드의 인덱스 (0xFFFFFFFFu이면 더 이상 없음)
     *
     * 노드 배열의 인덱스를 사용하므로 캐시 친화적인 순차 접근이 가능하다.
     */
    uint32_t next_index;

    /** @brief 값 타입 (leyn_bpb_cfg_value_type) */
    uint16_t value_type;

    /** @brief 노드 플래그 (leyn_bpb_cfg_node_flags 비트 OR) */
    uint16_t flags;

    /** @brief 키 문자열의 오프셋 (문자열 테이블 기준, 바이트 단위) */
    uint32_t key_offset;

    /**
     * @brief 값 데이터
     *
     * value_type에 따라 해석 방식이 달라진다.
     * - INT  : i64_value 사용
     * - UINT : u64_value 사용
     * - BOOL : u64_value의 하위 1비트 사용
     * - FLOAT: f64_value 사용
     * - STRING/BLOB/OBJECT/ARRAY/REF: u64_value를 오프셋/인덱스로 해석
     */
    union {
        int64_t  i64_value;  /**< 정수 값 */
        uint64_t u64_value;  /**< 부호 없는 정수 / 오프셋 / 인덱스 */
        double   f64_value;  /**< 실수 값 */
    } value;
};

/**
 * @brief CONFIG_GRAPH 섹션 상단에 위치하는 메타데이터
 *
 * CONFIG_GRAPH 섹션의 페이로드는 다음과 같이 구성된다.
 *
 * [ leyn_bpb_cfg_meta ]
 * [ leyn_bpb_cfg_node node_array[node_count] ]
 * [ 문자열 테이블 (null-terminated string들의 연속) ]
 * [ 바이너리 BLOB 영역 (leyn_bpb_cfg_blob_header + payload + tail_magic ... ) ]
 */
struct leyn_bpb_cfg_meta {
    /** @brief 노드 배열의 시작 오프셋 (섹션 페이로드 기준, 바이트 단위) */
    uint32_t node_array_offset;

    /** @brief 노드 개수 */
    uint32_t node_count;

    /** @brief 문자열 테이블의 시작 오프셋 (섹션 페이로드 기준, 바이트 단위) */
    uint32_t string_table_offset;

    /** @brief 문자열 테이블 크기 (바이트 단위) */
    uint32_t string_table_size;

    /** @brief BLOB 영역의 시작 오프셋 (섹션 페이로드 기준, 바이트 단위) */
    uint32_t blob_area_offset;

    /** @brief BLOB 영역 크기 (바이트 단위) */
    uint32_t blob_area_size;
};

/* -------------------------------------------------------------------------- */
/* CONFIG_GRAPH BLOB 포맷 + 모듈 시스템                                      */
/* -------------------------------------------------------------------------- */

/** @brief CONFIG_GRAPH 블롭 끝 검증용 매직 바이트 값 */
#define LEYN_BPB_CFG_BLOB_TAIL_MAGIC 0xA5u

/** @brief CONFIG_GRAPH 블롭 종류(kind) 상위 도메인 */
enum leyn_bpb_cfg_blob_kind {
    LEYN_BPB_CFG_BLOB_KIND_GENERIC = 0x00u, /* 일반 설정 블롭 */
    LEYN_BPB_CFG_BLOB_KIND_DTS     = 0x10u, /* DTS/디바이스 트리 관련 블롭 */
    LEYN_BPB_CFG_BLOB_KIND_IPC     = 0x20u, /* IPC 프로필/엔드포인트 관련 블롭 */
    LEYN_BPB_CFG_BLOB_KIND_MODULE  = 0x30u, /* 모듈/드라이버 정책 블롭 도메인 */
    /* 상위 4비트는 도메인, 하위 4비트는 서브타입/순서 등에 사용 가능 */
};

/** @brief MODULE 도메인 하위 서브타입 (kind 하위 4비트 사용 예시) */
enum leyn_bpb_cfg_blob_kind_module_sub {
    LEYN_BPB_CFG_BLOB_KIND_MODULE_POLICY = 0x31u, /* 모듈 로딩 정책/우선순위 */
    LEYN_BPB_CFG_BLOB_KIND_MODULE_CAPS   = 0x32u, /* 모듈 권한/캡어빌리티 정보 */
    LEYN_BPB_CFG_BLOB_KIND_MODULE_SIGN   = 0x33u, /* 모듈 서명/해시 정보 */
};

/** @brief CONFIG_GRAPH 블롭 헤더 (블롭 영역에 직접 저장됨) */
struct leyn_bpb_cfg_blob_header {
    uint8_t  kind;          /* 블롭 종류 및 순서 정보 (leyn_bpb_cfg_blob_kind 및 서브타입) */
    uint8_t  version;       /* 블롭 포맷 버전 (payload 해석용) */
    uint8_t  flags;         /* 블롭 개별 플래그 (압축/암호화 등 확장용) */
    uint8_t  reserved;      /* 정렬 및 미래 확장용 */
    uint32_t payload_size;  /* 페이로드 크기 (바이트 단위, tail 바이트는 포함하지 않음) */
    /* 이어서: uint8_t payload[payload_size]; */
    /* 그 뒤:  uint8_t tail_magic; // 항상 LEYN_BPB_CFG_BLOB_TAIL_MAGIC */
};

/** @brief 모듈 캡어빌리티 플래그 (MODULE_CAPS 블롭에서 사용) */
enum leyn_bpb_module_cap_flags {
    LEYN_BPB_MODCAP_ALLOW_IO_PORT   = (1u << 0), /* IO 포트 접근 허용 */
    LEYN_BPB_MODCAP_ALLOW_MMIO      = (1u << 1), /* MMIO 영역 접근 허용 */
    LEYN_BPB_MODCAP_ALLOW_DMA       = (1u << 2), /* DMA 사용 허용 */
    LEYN_BPB_MODCAP_ALLOW_IRQ       = (1u << 3), /* IRQ 핸들링 허용 */
    LEYN_BPB_MODCAP_ALLOW_PRIV      = (1u << 4), /* 특권 명령어/커널 API 사용 허용 */
    LEYN_BPB_MODCAP_REQUIRE_SIGNED  = (1u << 5), /* 서명 검증 필수 */
    LEYN_BPB_MODCAP_HARD_RT         = (1u << 6), /* 실시간 제약 하에서 동작하는 모듈 */
};

/**
 * @brief 모듈 캡어빌리티 구조체 (MODULE_CAPS 블롭 payload의 예시 포맷)
 *
 * 이 구조체는 CONFIG_GRAPH BLOB 안에 직접 임베드되어 사용된다.
 * Leyn 커널은 이 정보를 바탕으로 모듈의 권한 및 리소스 범위를 제한할 수 있다.
 */
struct leyn_bpb_module_caps {
    uint32_t api_version;     /* 모듈 ABI/API 버전 */
    uint32_t flags;           /* leyn_bpb_module_cap_flags 비트 OR */

    uint64_t allowed_irq_mask;    /* 허용된 IRQ 비트마스크(플랫폼 정의) */

    uint64_t io_port_base;        /* 허용된 IO 포트 베이스 (옵션) */
    uint64_t io_port_count;       /* 허용된 IO 포트 개수 */

    uint64_t mmio_base;           /* 허용된 MMIO 베이스(물리 주소) */
    uint64_t mmio_size;           /* 허용된 MMIO 크기(바이트) */
};

/* -------------------------------------------------------------------------- */
/* 마무리                                                                    */
/* -------------------------------------------------------------------------- */

#ifdef __cplusplus
}
#endif

#endif /* LEYN_BPB_H */

```

