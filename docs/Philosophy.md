# Ribon Boot Framework – Design Philosophy

---

## 1. Core is Immutable (불변 코어)

Ribon의 핵심은 절대 변하지 않는 "Core Layer"이다. 이 Core는 플랫폼이나 특정 커널에 종속된 로직을 포함하지 않는다.

### Core가 하지 않는 것

* Fiasco 부팅 규칙
* Multiboot 규칙
* Linux 커널 규칙
* 특정 OS 전용 처리

### Core가 하는 것

* ELF 로딩 (순수 ELF 로더)
* UEFI File/Memory/Graphics wrapper
* Low-level utilities (memcpy, print, allocator 등)
* Architecture primitives (GDT, paging, stack switching 등)

> 핵심 원칙: **어떤 커널을 추가해도 Core는 변경되지 않는다.**

---

## 2. Everything Above Core is a Module (모든 부트 정책은 모듈)

커널을 부팅하는 모든 코드는 Module 형태로 제공된다. Module은 Core에 의존하지만 Core는 Module에 의존하지 않는다.

### Module의 예시

* FiascoModule → Fiasco KIP 생성, memdesc 변환, module table 구축
* MultibootModule → MB1/MB2 구조블록 생성
* 향후 LinuxModule, ZirconModule 등 추가 가능

### 모듈 규칙

* 각 모듈은 **독립 CMake**로 빌드됨
* 각 모듈은 독립 정적 라이브러리로 제공됨
* Core를 수정하지 않고 모듈만 추가하여 기능 확장 가능

---

## 3. Independent Build Units (독립 컴파일 단위)

Ribon의 폴더 구조는 모든 기능이 독립 정적 라이브러리로 컴파일되도록 설계되어 있다.

### 예시

```
src/elf       → libribon_elf.a
src/efi       → libribon_efi.a
src/multiboot → libribon_mb.a
src/fiasco    → libribon_fiasco.a
```

최종적으로 Core + 모듈 + 아키텍처 라이브러리를 모두 합쳐:

```
BOOTX64.EFI (또는 BOOTAA64.EFI)
```

을 생성한다.

---

## 4. Architecture Separation (아키텍처 분리)

Ribon은 다음과 같은 구조를 가진다:

```
arch/x86_64
arch/aarch64
arch/riscv64
```

각 아키텍처 폴더는 다음을 포함한다:

* UEFI 엔트리
* 초기 paging / GDT 구성
* switch_to_kernel.S
* 플랫폼별 레지스터/부트 규약 처리

Architecture 레이어는 Core와는 별개의 역할을 갖지만 Core의 일부처럼 동작한다.

---

## 5. Final Composition (최종 조립 과정)

모든 라이브러리는 다음 순서로 조립된다:

1. libribon_core.a
2. libribon_elf.a
3. libribon_efi.a
4. 모듈(Fiasco, MB1, MB2 등)
5. 아키텍처 라이브러리

그리고 하나의 EFI 바이너리로 통합된다.

---

## 6. Replaceable Boot Policies (부팅 정책 교체 가능)

Ribon의 모듈 시스템은 다음을 쉽게 만든다:

* Fiasco 전용 로직을 제거·수정
* Multiboot v1/v2 지원 추가 또는 제거
* Linux/Zircon/Custom OS 모듈 추가

Core는 어떤 경우에도 수정되지 않는다.

---

## 7. Ribon is a Boot Framework, not a Bootloader

Ribon은 단순한 "부트로더"가 아니다.

### Ribon은 다음을 제공하는 프레임워크이다:

* 부트 인프라
* 공통 라이브러리
* 아키텍처 초기화 계층
* OS/커널별 모듈 시스템
* ELF/UEFI/HAL abstraction layer

즉, Ribon 자체는 완전한 제품이 아니며, **부트 기능을 구현하기 위한 기반 구조**다.

---

## 8. Long-term Goal

Ribon의 장기적 목표는:

* 어떠한 OS/커널/하이퍼바이저도 Ribon 모듈만 작성하면 부팅 가능
* Core는 절대 변하지 않는 최소 부트 인프라
* 모듈만 확장해 무한 확장성 확보
* 진입점: BOOTX64.EFI

---

## **최종 요약**

Ribon의 철학을 한 문장으로 표현하면 다음과 같다:

### **"Core는 불변이고, 모든 부트 정책은 모듈화된다.

모듈은 독립 빌드·독립 라이브러리이며,
새로운 커널은 Core를 변경하지 않고 모듈만 작성하면 된다."**
