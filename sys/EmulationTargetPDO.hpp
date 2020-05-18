/*
* Virtual Gamepad Emulation Framework - Windows kernel-mode bus driver
*
* MIT License
*
* Copyright (c) 2016-2020 Nefarius Software Solutions e.U. and Contributors
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#pragma once

#include <ntddk.h>
#include <wdf.h>
#include <ntintsafe.h>

#include <usb.h>
#include <usbbusif.h>

#include <ViGEm/Common.h>
#include <initguid.h>

//
// Some insane macro-magic =3
// 
#define P99_PROTECT(...) __VA_ARGS__
#define COPY_BYTE_ARRAY(_dst_, _bytes_)   do {BYTE b[] = _bytes_; \
                                            RtlCopyMemory(_dst_, b, RTL_NUMBER_OF_V1(b)); } while (0)

namespace ViGEm::Bus::Core
{
	typedef struct _PDO_IDENTIFICATION_DESCRIPTION* PPDO_IDENTIFICATION_DESCRIPTION;

	class EmulationTargetPDO
	{
	public:
		EmulationTargetPDO(ULONG Serial, LONG SessionId, USHORT VendorId, USHORT ProductId);

		virtual ~EmulationTargetPDO() = default;
		
		static bool GetPdoByTypeAndSerial(
			IN WDFDEVICE ParentDevice,
			IN VIGEM_TARGET_TYPE Type,
			IN ULONG SerialNo, 
			OUT EmulationTargetPDO** Object
		);

		virtual NTSTATUS PdoPrepareDevice(PWDFDEVICE_INIT DeviceInit,
		                                  PUNICODE_STRING DeviceId,
		                                  PUNICODE_STRING DeviceDescription) = 0;

		virtual NTSTATUS PdoPrepareHardware() = 0;

		virtual NTSTATUS PdoInitContext() = 0;

		NTSTATUS PdoCreateDevice(_In_ WDFDEVICE ParentDevice,
		                         _In_ PWDFDEVICE_INIT DeviceInit);

		bool operator==(EmulationTargetPDO& other) const
		{
			return (other._SerialNo == this->_SerialNo);
		}

		virtual NTSTATUS UsbGetDeviceDescriptorType(PUSB_DEVICE_DESCRIPTOR pDescriptor) = 0;

		NTSTATUS UsbSelectConfiguration(PURB Urb);

		void UsbAbortPipe();

		NTSTATUS UsbGetConfigurationDescriptorType(PURB Urb);

		virtual NTSTATUS UsbClassInterface(PURB Urb) = 0;

		virtual NTSTATUS UsbGetDescriptorFromInterface(PURB Urb) = 0;

		virtual NTSTATUS UsbSelectInterface(PURB Urb) = 0;

		virtual NTSTATUS UsbGetStringDescriptorType(PURB Urb) = 0;

		virtual NTSTATUS UsbBulkOrInterruptTransfer(struct _URB_BULK_OR_INTERRUPT_TRANSFER* pTransfer,
		                                            WDFREQUEST Request) = 0;

		virtual NTSTATUS UsbControlTransfer(PURB Urb) = 0;

		NTSTATUS SubmitReport(PVOID NewReport);

		NTSTATUS EnqueueNotification(WDFREQUEST Request) const;

		bool IsOwnerProcess() const;

		VIGEM_TARGET_TYPE GetType() const;

		NTSTATUS EnqueuePlugin(WDFREQUEST Request);

		NTSTATUS PdoPrepare(WDFDEVICE ParentDevice);
	
	private:
		static unsigned long current_process_id();

		static EVT_WDF_DEVICE_CONTEXT_CLEANUP EvtDeviceContextCleanup;

		static bool GetPdoBySerial(
			IN WDFDEVICE ParentDevice,
			IN ULONG SerialNo,
			OUT EmulationTargetPDO** Object
		);

		HANDLE _PluginRequestCompletionWorkerThreadHandle{};
		
	protected:
		static const ULONG _maxHardwareIdLength = 0xFF;

		static const int MAX_INSTANCE_ID_LEN = 80;
		
		static PCWSTR _deviceLocation;

		static BOOLEAN USB_BUSIFFN UsbInterfaceIsDeviceHighSpeed(IN PVOID BusContext);

		static NTSTATUS USB_BUSIFFN UsbInterfaceQueryBusInformation(
			IN PVOID BusContext,
			IN ULONG Level,
			IN OUT PVOID BusInformationBuffer,
			IN OUT PULONG BusInformationBufferLength,
			OUT PULONG BusInformationActualLength
		);

		static NTSTATUS USB_BUSIFFN UsbInterfaceSubmitIsoOutUrb(IN PVOID BusContext, IN PURB Urb);

		static NTSTATUS USB_BUSIFFN UsbInterfaceQueryBusTime(IN PVOID BusContext, IN OUT PULONG CurrentUsbFrame);

		static VOID USB_BUSIFFN UsbInterfaceGetUSBDIVersion(
			IN PVOID BusContext,
			IN OUT PUSBD_VERSION_INFORMATION VersionInformation,
			IN OUT PULONG HcdCapabilities
		);

		static EVT_WDF_DEVICE_PREPARE_HARDWARE EvtDevicePrepareHardware;

		static EVT_WDF_IO_QUEUE_IO_INTERNAL_DEVICE_CONTROL EvtIoInternalDeviceControl;

		static VOID PluginRequestCompletionWorkerRoutine(IN PVOID StartContext);
		
		virtual VOID GetConfigurationDescriptorType(PUCHAR Buffer, ULONG Length) = 0;

		virtual NTSTATUS SelectConfiguration(PURB Urb) = 0;

		virtual void AbortPipe() = 0;

		virtual NTSTATUS SubmitReportImpl(PVOID NewReport) = 0;

		static VOID DumpAsHex(PCSTR Prefix, PVOID Buffer, ULONG BufferLength);

		//
		// PNP Capabilities may differ from device to device
		// 
		WDF_DEVICE_PNP_CAPABILITIES _PnpCapabilities;

		//
		// Power Capabilities may differ from device to device
		// 
		WDF_DEVICE_POWER_CAPABILITIES _PowerCapabilities;
		
		//
		// Unique serial number of the device on the bus
		// 
		ULONG _SerialNo{};

		// 
		// PID of the process creating this PDO
		// 
		DWORD _OwnerProcessId{};

		//
		// File object session ID
		// 
		LONG _SessionId{};

		//
		// Device type this PDO is emulating
		// 
		VIGEM_TARGET_TYPE _TargetType;

		//
		// If set, the vendor ID the emulated device is reporting
		// 
		USHORT _VendorId{};

		//
		// If set, the product ID the emulated device is reporting
		// 
		USHORT _ProductId{};

		//
		// Queue for blocking plugin requests
		// 
		WDFQUEUE _PendingPlugInRequests{};
		
		//
		// Queue for incoming data interrupt transfer
		//
		WDFQUEUE _PendingUsbInRequests{};

		//
		// Queue for inverted calls
		//
		WDFQUEUE _PendingNotificationRequests{};

		//
		// This child objects' device object
		// 
		WDFDEVICE _PdoDevice{};
		
		//
		// Configuration descriptor size (populated by derived class)
		// 
		ULONG _UsbConfigurationDescriptionSize{};

		//
		// Signals the bus that PDO is ready to receive data
		// 
		KEVENT _PdoBootNotificationEvent;
	};

	typedef struct _PDO_IDENTIFICATION_DESCRIPTION
	{
		//
		// List entity header
		// 
		WDF_CHILD_IDENTIFICATION_DESCRIPTION_HEADER Header;

		//
		// Primary key to identify PDO
		// 
		ULONG SerialNo;

		//
		// Session ID
		// 
		LONG SessionId;

		//
		// Context object of PDO
		// 
		EmulationTargetPDO* Target;
	} PDO_IDENTIFICATION_DESCRIPTION, *PPDO_IDENTIFICATION_DESCRIPTION;

	typedef struct _EMULATION_TARGET_PDO_CONTEXT
	{
		EmulationTargetPDO* Target;
	} EMULATION_TARGET_PDO_CONTEXT, *PEMULATION_TARGET_PDO_CONTEXT;

	WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(EMULATION_TARGET_PDO_CONTEXT, EmulationTargetPdoGetContext)
}
