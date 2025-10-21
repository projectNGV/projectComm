from udsoncan.client import Client

from udsoncan.exceptions import NegativeResponseException

from .uds import *
from .hex_parser import get_block


def run_ota_process(client: Client, filepath: str, update_result_text):
    update_result_text("시작")
    structured_data = get_block(filepath)
    if not structured_data:
        print("❌ Error: Failed to process firmware data. Stopping the process.")
        return
    print(f"✅ Parsing complete: Found {len(structured_data)} Chunks.")
    try:
        diagnostic_session_control(client, 0x02, update_result_text)  # Switch to programming session
        for i, sector in enumerate(structured_data):
            routine_control_erase_flash(client, sector['sector_start_addr'], sector['num_sectors'] * 0x4000, update_result_text)
            for i, block in enumerate(sector['block']):
                request_download(client, block['address'], len(block['data']), update_result_text)
                transfer_data_blob(client, block['data'], update_result_text)
                request_transfer_exit(client, update_result_text)
                print(f"✅ Block {i+1}/{len(sector['block'])} in Sector {i+1}/{len(structured_data)} flashed successfully.")
            print(f"✅ Sector {i+1}/{len(structured_data)} flashed successfully.")
        ecu_reset(client, 0x01, update_result_text)  # Hard Reset              

        print("🎉 Firmware update process completed successfully!")
    except NegativeResponseException as e:
        print(f"❌ Negative response from ECU: Service={e.response.service.name}, Code={e.response.code_name} ({hex(e.response.code)})")
        return False
    except Exception as e:
        print(f"\n🚨 Error: {e}")