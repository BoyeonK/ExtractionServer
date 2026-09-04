# Extraction Shooter

Linux C++ 게임 서버와 Unity 클라이언트로 구축된 멀티플레이어 익스트랙션 슈터 게임 프로젝트입니다.
이 프로젝트는 비동기 네트워킹, 매치메이킹, 데디케이티드 서버, DB 연동 등을 설계 및 구현하고, 클라우드 환경으로의 배포함으로서 닫힌 환경에서 작동한다고 '주장'하는것이 아닌 실제로 플레이 가능한 상태의 온라인 멀티플레이어 게임을 구현하는 데 중점을 두었습니다.

[다운로드 구글 드라이브 링크]

[게임 플레이 GIF / 영상]

## Overview

이 프로젝트는 게임 서버 아키텍처와 클라이언트 사이의 네트워킹 시스템을 직접 구현하기 위해 1인 개발로 진행된 멀티플레이어 게임 프로젝트의 서버측 부분입니다.
프로젝트를 단순히 로컬 환경에서 동작한다 수준이 아니라, 스푸핑, 스니핑등의 위협이 있는 실제 공개된 환경에서 구동 가능한 멀티플레이어 게임을 목표로 합니다.

프로젝트에서 강조하고 싶은 영역들입니다.
- Linux C++ 멀티플레이어 서버 아키텍쳐
    - io_uring을 활용한 비동기 네트워킹
    - 메인 서버(C++), HTTPS API 서버(js), 그리고 다수의 데디케이티드 게임 서버 프로세스로 구성
- 커스텀 RUDP 전송 프로토콜
    - ACK 및 재전송 메커니즘을 가진 Reliable 채널 / 패킷 로스를 감수하는 Unreliable 채널 분리
    - HTTPS통신을 통해 교환된 키를 사용한 해싱을 통해 위, 변조 방지
- 매치메이킹 시스템
    - 플레이어의 공격성과 매치 대기 시간을 기반으로 한 매칭 시스템
- 동적 데디케이티드 프로세스 생성 및 관리
    - 액티브 유저 수에 따라 데디케이티드 프로세스를 동적으로 생성, 할당
- 퍼블릭 클라우드 환경으로의 배포
    - Cloudflare 리버스 프록시, Oracle Compute Instance, Redis 및 MySQL HeatWave 연동

## Architecture

![ExtractionServer Architecture](docs/diagrams/architecture.svg)

## Engineering Highlights

### 1. Custom RUDP Transport
### 2. io_uring Async Networking
### 3. Matchmaking System
### 4. Dynamic Dedicated Server

## Public Cloud Deployment

## Gameplay
[영상 / 스크린샷]

## Additional Work
- Unity Client
- Generative AI Asset Pipeline

## Tech Stack

## Documentation
- Architecture
- Networking
- Matchmaking
- Dedicated Server
- Deployment

## Repository
Client / Server