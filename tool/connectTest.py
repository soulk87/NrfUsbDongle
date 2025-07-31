import hid
import time

# --- 장치 정보 설정 (!!! 사용자의 장치에 맞게 수정하세요 !!!) ---
# 예시: QMK/VIA 기반 커스텀 키보드에서 자주 사용되는 값
VENDOR_ID = 0x0483
PRODUCT_ID = 0x5220

# --- 리포트 디스크립터에서 정의된 값 ---
REPORT_ID_VIA = 0x03
# VIA 리포트의 데이터 크기는 64바이트로 정의되어 있습니다.
VIA_REPORT_SIZE = 64

def find_hid_device(vid, pid):
    """지정된 VID, PID를 가진 HID 장치를 찾습니다."""
    try:
        device = hid.device()
        device.open(vid, pid)
        # VIA 기능이 있는 장비는 보통 Usage Page가 0xFF60 입니다.
        # 더 정확하게 찾으려면 product_string, usage_page 등을 확인할 수 있습니다.
        print(f"장치 찾음: {device.get_product_string()} - {device.get_manufacturer_string()}")
        return device
    except IOError as e:
        print(f"오류: 장치를 찾을 수 없습니다. (VID={hex(vid)}, PID={hex(pid)})")
        print("장치가 연결되었는지, VID/PID가 올바른지 확인하세요.")
        return None

def send_via_command(device, command_data):
    """
    VIA 리포트를 사용해 장치로 데이터를 보냅니다. (Output Report)
    
    주의: hidapi의 write 메소드는 첫 바이트에 Report ID를 포함해야 합니다.
    따라서 [Report ID] + [Data] 형태로 총 65바이트를 보냅니다.
    """
    if len(command_data) != VIA_REPORT_SIZE:
        print(f"오류: 데이터는 정확히 {VIA_REPORT_SIZE}바이트여야 합니다.")
        return False
        
    # 전송할 버퍼 생성 (Report ID + 데이터)
    buffer = bytearray([REPORT_ID_VIA]) + bytearray(command_data)
    
    try:
        print(f"-> 장치로 데이터 전송 (총 {len(buffer)} 바이트)")
        device.write(buffer)
        return True
    except IOError as e:
        print(f"데이터 전송 실패: {e}")
        return False

def receive_via_response(device, timeout_ms=1000):
    """
    VIA 리포트를 사용해 장치로부터 데이터를 받습니다. (Input Report)
    """
    try:
        # 리포트 ID(1바이트) + 데이터(64바이트) = 65바이트
        print("<- 장치로부터 데이터 수신 대기...")
        response = device.read(VIA_REPORT_SIZE + 1, timeout=timeout_ms)
        
        if not response:
            print("응답 시간 초과 (Timeout)")
            return None
        
        # 첫 바이트가 VIA Report ID인지 확인
        if response[0] == REPORT_ID_VIA:
            # Report ID를 제외한 실제 데이터만 반환
            return response[1:]
        else:
            print(f"경고: 예상치 못한 Report ID 수신 (ID: {hex(response[0])})")
            return None
            
    except IOError as e:
        print(f"데이터 수신 실패: {e}")
        return None


if __name__ == "__main__":
    # 장치 열기
    h = find_hid_device(VENDOR_ID, PRODUCT_ID)

    if h:
        try:
            # 1. 장치로 보낼 64바이트 데이터 준비 (예시)
            #    실제 데이터는 장치의 펌웨어와 약속된 프로토콜에 따라 달라집니다.
            #    예: 0x01은 펌웨어 정보 요청을 의미할 수 있습니다.
            my_command = [0x01] + [0] * (VIA_REPORT_SIZE - 1)

            # 2. 데이터 전송
            if send_via_command(h, my_command):
                
                time.sleep(0.1) # 장치가 처리할 시간을 약간 줍니다.

                # 3. 장치로부터 응답 수신
                response_data = receive_via_response(h)
                
                if response_data:
                    print(f"수신된 데이터 ({len(response_data)} 바이트):")
                    # 보기 좋게 16진수로 출력
                    print(' '.join(f'{b:02x}' for b in response_data))

        finally:
            # 4. 장치 닫기
            print("장치를 닫습니다.")
            h.close()