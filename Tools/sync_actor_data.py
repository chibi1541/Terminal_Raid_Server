# -*- coding: utf-8 -*-
"""
액터 정의 XML 을 서버 원본에서 클라이언트로 복제한다.

    python Server/Tools/sync_actor_data.py

원본 : Server/Data/<name>.xml        (여기만 손으로 고친다)
복제 : Client/Assets/Actor/<name>.xml

서버는 ../Data/<name>.xml 로 직접 읽고,
클라는 ActorData.xml(인덱스)이 가리키는 ../Assets/Actor/<name>.xml 로 읽는다.
두 exe 의 작업 디렉터리가 달라 한 파일을 공유할 수 없으므로 복사한다.

빌드에는 넣지 않는다. 수치를 바꿨을 때만 돌리고 두 파일을 함께 커밋한다.
"""

import os

REPO_ROOT = os.path.normpath(os.path.join(os.path.dirname(__file__), "..", ".."))
SRC_DIR = os.path.join(REPO_ROOT, "Server", "Data")
DST_DIR = os.path.join(REPO_ROOT, "Client", "Assets", "Actor")

# 서버 원본 -> 클라 복제 대상. ActorData.xml(클라 전용 인덱스)과 Level01.xml(서버 전용)은 제외.
FILES = [
    "ProjectileData.xml",
    "CharacterData.xml",
    "MonsterData.xml",
]


def main():
    os.makedirs(DST_DIR, exist_ok=True)

    for name in FILES:
        src = os.path.join(SRC_DIR, name)
        dst = os.path.join(DST_DIR, name)

        with open(src, "r", encoding="utf-8", newline="") as f:
            text = f.read().replace("\r\n", "\n")

        with open(dst, "w", encoding="utf-8", newline="\n") as f:
            f.write(text)

        print(f"{src}\n  -> {dst}")


if __name__ == "__main__":
    main()
