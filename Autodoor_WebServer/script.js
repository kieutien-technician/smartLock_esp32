
/* Firebase Configuration */
const firebaseConfig = {
    apiKey: "AIzaSyAC0w_YgXdwKKjtwk1vzV8aljxAYL2BPEM",
    authDomain: "automatic-door-using-rfid.firebaseapp.com",
    databaseURL: "https://automatic-door-using-rfid-default-rtdb.firebaseio.com",
    projectId: "automatic-door-using-rfid",
    storageBucket: "automatic-door-using-rfid.firebasestorage.app",
    messagingSenderId: "89109537054",
    appId: "1:89109537054:web:b637d77d9303d15ec3e90f"
};

/* Firebase Initialization */
firebase.initializeApp(firebaseConfig);
const db = firebase.database();

/* DOM Elements */
const lockIcon = document.getElementById("lock-status");
const statusText = document.getElementById("status-text");
const unlockBtn = document.getElementById("unlock-btn");
const lockBtn = document.getElementById("lock-btn");
const historyList = document.getElementById("history-list");
const clearHistoryBtn = document.getElementById("clear-history-btn");
const historyToggle = document.getElementById("history-toggle");
const historyContainer = document.getElementById("history-container");

/* Lock Status Monitoring */
db.ref("/SmartLock/smartlock_state/smartlock_state").on("value", (snapshot) => {
    const smartlock_state = snapshot.val();
    updateLockUI(smartlock_state);
});

/* Access History */
db.ref("/SmartLock/smartlock_history").limitToLast(100).on("value", (snapshot) => {
    historyList.innerHTML = "";
    const data = snapshot.val();

    if (data) {
        const items = Object.entries(data).reverse();

        items.forEach(([key, val]) => {
            const li = document.createElement("li");
            li.textContent = `${val.time} - ${val.method} (${val.id})`;
            historyList.appendChild(li);
        });
    }
});

/* Lock UI Update */
function updateLockUI(state) {
    if (state === 1) {
        lockIcon.textContent = "🔓";
        lockIcon.classList.add("open");
        statusText.textContent = "Đã mở khóa";
    } else {
        lockIcon.textContent = "🔒";
        lockIcon.classList.remove("open");
        statusText.textContent = "Đang khóa";
    }
}

/* Vietnam Time Formatting */
function getFormattedVNTime() {
    const now = new Date(new Date().toLocaleString("en-US", {
        timeZone: "Asia/Ho_Chi_Minh"
    }));

    const pad = (n) => n.toString().padStart(2, '0');

    return `${now.getFullYear()}-${pad(now.getMonth() + 1)}-${pad(now.getDate())} ` +
           `${pad(now.getHours())}:${pad(now.getMinutes())}:${pad(now.getSeconds())}`;
}

/* Firebase Lock State Synchronization */
db.ref("/SmartLock/smartlock_state/smartlock_state").on("value", function(snapshot) {
    const smartlock_state = snapshot.val();

    if (smartlock_state == 0) {
        db.ref("/SmartLock/RFID/rfid_signal").update({
            rfid_signal_smartlock: 0
        });

        db.ref("/SmartLock/FINGERPRINT/fingerprint_signal").update({
            fingerprint_signal_smartlock: 0
        });

        updateLockUI(0);
    } else {
        updateLockUI(1);
    }
});

/* Web Lock Control */
unlockBtn.onclick = () => {
    const formattedTime = getFormattedVNTime();
    const historyKey = `${formattedTime}-web_unlock`;

    db.ref("/SmartLock/smartlock_state").update({
        smartlock_state: 1
    });

    db.ref("/SmartLock/smartlock_history/" + historyKey).set({
        time: formattedTime,
        method: "Web",
        id: "Admin",
        action: "unlock"
    });

    updateLockUI(1);
};

lockBtn.onclick = () => {
    const formattedTime = getFormattedVNTime();
    const historyKey = `${formattedTime}-web_lock`;

    db.ref("/SmartLock/smartlock_state").update({
        smartlock_state: 0
    });

    db.ref("/SmartLock/smartlock_history/" + historyKey).set({
        time: formattedTime,
        method: "Web",
        id: "Admin",
        action: "lock"
    });

    updateLockUI(0);
};

/* RFID Authentication */
db.ref("/SmartLock/RFID/rfid_signal/rfid_signal_smartlock").on("value", function(snapshot) {
    const rfid_signal_smartlock = snapshot.val();
    const formattedTime = getFormattedVNTime();

    if (rfid_signal_smartlock == 1) {
        db.ref("/SmartLock").update({
            smartlock_state: 1
        });

        db.ref("/SmartLock/RFID/rfid_id_card_handle/rfid_id_card_unlock").once("value", function(snapshot) {
            const rfidID = snapshot.val();
            const historyKey = `${formattedTime}-rfid_unlock`;

            db.ref("/SmartLock/smartlock_history/" + historyKey).set({
                time: formattedTime,
                method: "RFID",
                id: rfidID,
                action: "unlock"
            });
        });

        updateLockUI(1);
    }
});

/* RFID Registration */
db.ref("/SmartLock/RFID/rfid_signal/rfid_signal_new").on("value", function(snapshot) {
    const rfid_signal_import = snapshot.val();

    if (rfid_signal_import == 1) {
        db.ref("/SmartLock/RFID/rfid_id_card_handle/rfid_id_card_new").once("value", function(snapshot) {
            const rfid_id_card_import = snapshot.val();

            db.ref("/SmartLock/RFID/rfid_card_storage").once("value", function(snapshot) {
                const currentData = snapshot.val() || {};
                const newKey = "id_card_" + rfid_id_card_import;

                db.ref("/SmartLock/RFID/rfid_card_storage/" + newKey).once("value", function(snapshot) {
                    if (snapshot.exists()) {
                        alert("Thẻ đã tồn tại: " + rfid_id_card_import);
                    } else {
                        currentData["id_card_" + rfid_id_card_import] = rfid_id_card_import;

                        alert("Thêm thẻ RFID thành công: " + rfid_id_card_import);

                        db.ref("/SmartLock/RFID/rfid_card_storage").set(currentData);
                    }
                });
            });
        });
    }
});

/* RFID Deletion */
db.ref("/SmartLock/RFID/rfid_signal/rfid_signal_delete").on("value", function(snapshot) {
    const rfid_signal_delete = snapshot.val();

    if (rfid_signal_delete == 1) {
        db.ref("/SmartLock/RFID/rfid_id_card_handle/rfid_id_card_delete").once("value", function(snapshot) {
            const rfid_id_card_delete = snapshot.val();

            if (rfid_id_card_delete && rfid_id_card_delete.trim() !== "") {
                const deleteKey = "id_card_" + rfid_id_card_delete;

                db.ref("/SmartLock/RFID/rfid_card_storage/" + deleteKey).once("value", function(snapshot) {
                    if (snapshot.exists()) {
                        db.ref("/SmartLock/RFID/rfid_card_storage/" + deleteKey).remove()
                            .then(() => {
                                alert("Xóa thẻ RFID thành công: " + rfid_id_card_delete);
                            })
                            .catch((error) => {
                                alert("Lỗi khi xóa thẻ RFID: " + error.message);
                            });
                    } else {
                        alert("Không tìm thấy thẻ RFID để xóa: " + rfid_id_card_delete);
                    }
                });
            } else {
                alert("Không có id_card cần xóa hoặc giá trị id_card rỗng.");
            }
        });
    }
});

/* Fingerprint Authentication */
db.ref("/SmartLock/FINGERPRINT/fingerprint_signal/fingerprint_signal_smartlock").on("value", function(snapshot) {
    const fingerprint_signal_smartlock = snapshot.val();
    const formattedTime = getFormattedVNTime();

    if (fingerprint_signal_smartlock == 1) {
        db.ref("/SmartLock").update({
            smartlock_state: 1
        });

        db.ref("/SmartLock/FINGERPRINT/fingerprint_id_handle/fingerprint_id_unlock").once("value", function(snapshot) {
            const fingerprintID = snapshot.val();
            const historyKey = `${formattedTime}-fingerprint_unlock`;

            db.ref("/SmartLock/smartlock_history/" + historyKey).set({
                time: formattedTime,
                method: "FINGERPRINT",
                id: fingerprintID,
                action: "unlock"
            });
        });

        updateLockUI(1);
    }
});

/* Fingerprint Registration */
db.ref("/SmartLock/FINGERPRINT/fingerprint_signal/fingerprint_signal_new").on("value", function(snapshot) {
    const fingerprint_signal_new = snapshot.val();

    if (fingerprint_signal_new == 1) {
        db.ref("/SmartLock/FINGERPRINT/fingerprint_id_handle/fingerprint_id_new").once("value", function(snapshot) {
            const fingerprint_id_new = snapshot.val();

            db.ref("/SmartLock/FINGERPRINT/fingerprint_storage").once("value", function(snapshot) {
                const currentData = snapshot.val() || {};
                const newKey = "id_fingerprint_" + fingerprint_id_new;

                db.ref("/SmartLock/FINGERPRINT/fingerprint_storage/" + newKey).once("value", function(snapshot) {
                    if (snapshot.exists()) {
                        alert("Vân tay đã tồn tại: " + fingerprint_id_new);
                    } else {
                        currentData["id_fingerprint_" + fingerprint_id_new] = fingerprint_id_new;

                        alert("Thêm vân tay thành công: " + fingerprint_id_new);

                        db.ref("/SmartLock/FINGERPRINT/fingerprint_storage").set(currentData);
                    }
                });
            });
        });
    }
});

/* Fingerprint Deletion */
db.ref("/SmartLock/FINGERPRINT/fingerprint_signal/fingerprint_signal_delete").on("value", function(snapshot) {
    const fingerprint_signal_delete = snapshot.val();

    if (fingerprint_signal_delete == 1) {
        db.ref("/SmartLock/FINGERPRINT/fingerprint_id_handle/fingerprint_id_delete").once("value", function(snapshot) {
            const fingerprint_id_delete = snapshot.val();

            if (fingerprint_id_delete && fingerprint_id_delete.trim() !== "") {
                const deleteKey = "id_fingerprint_" + fingerprint_id_delete;

                db.ref("/SmartLock/FINGERPRINT/fingerprint_storage/" + deleteKey).once("value", function(snapshot) {
                    if (snapshot.exists()) {
                        db.ref("/SmartLock/FINGERPRINT/fingerprint_storage/" + deleteKey).remove()
                            .then(() => {
                                alert("Xóa vân tay thành công: " + fingerprint_id_delete);
                            })
                            .catch((error) => {
                                alert("Lỗi khi xóa vân tay: " + error.message);
                            });
                    } else {
                        alert("Không tìm thấy vân tay để xóa: " + fingerprint_id_delete);
                    }
                });
            } else {
                alert("Không có id_card cần xóa hoặc giá trị id_card rỗng.");
            }
        });
    }
});

/* Master Card ID Monitoring */
let prevMasterID = null;

db.ref("SmartLock/smartlock_mastercard/smartlock_mastercard_id").on("value", function(snapshot) {
    const newID = snapshot.val();

    if (prevMasterID === null) {
        prevMasterID = newID;
        return;
    }

    if (newID !== prevMasterID) {
        prevMasterID = newID;
        alert("Cập nhật ID_MASTER thành công");
    }
});

/* Warning Management */
let isWarningOn = false;

/* Web Warning Control */
document.getElementById("alert-toggle-btn").onclick = function() {
    isWarningOn = !isWarningOn;

    const formattedTime = getFormattedVNTime();
    const warningKey = formattedTime + "-web_warning";

    if (isWarningOn) {
        db.ref("/SmartLock/smartlock_warning/smartlock_warning_history").once("value", function(snapshot) {
            const currentData = snapshot.val() || {};

            currentData[warningKey] = {
                time: new Date().toLocaleString(),
                message: "Cảnh báo từ web: Phát hiện bất thường!"
            };

            db.ref("/SmartLock/smartlock_warning/smartlock_warning_history").set(currentData);
        });

        alert("Đã bật cảnh báo!");
    } else {
        alert("Đã tắt cảnh báo!");
    }

    db.ref("/SmartLock/smartlock_warning").update({
        "smartlock_warning_buzzer": isWarningOn ? 1 : 0
    });

    document.getElementById("alert-toggle-btn").textContent = isWarningOn ? "Tắt Cảnh Báo" : "Bật Cảnh Báo";
    document.getElementById("alert-status").textContent = isWarningOn ? "Cảnh báo đang bật" : "Cảnh báo đã tắt";
};

/* Microcontroller Warning Monitoring */
db.ref("/SmartLock/smartlock_warning/smartlock_warning_signal").on("value", function(snapshot) {
    const smartlock_warning_signal = snapshot.val();

    if (smartlock_warning_signal == 1) {
        const formattedTime = getFormattedVNTime();
        const warningKey = formattedTime + "-microcontroller_warning";

        db.ref("/SmartLock/smartlock_warning/smartlock_warning_history").once("value", function(snapshot) {
            const currentData = snapshot.val() || {};

            currentData[warningKey] = {
                time: new Date().toLocaleString(),
                message: "Cảnh báo từ microcontroller: Phát hiện bất thường!"
            };

            db.ref("/SmartLock/smartlock_warning/smartlock_warning_history").set(currentData);

            db.ref("/SmartLock/smartlock_warning").update({
                "smartlock_warning_buzzer": 1
            });

            isWarningOn = true;
            document.getElementById("alert-toggle-btn").textContent = "Tắt Cảnh Báo";
            document.getElementById("alert-status").textContent = "Cảnh báo đang bật";

            alert("Cảnh báo từ hệ thống!");
        });
    }
});

/* History Display Control */
historyToggle.addEventListener("click", () => {
    const isExpanded = historyContainer.classList.toggle("expanded");

    if (isExpanded) {
        historyToggle.textContent = "Lịch sử đóng/mở cửa ⬆️";
    } else {
        historyToggle.textContent = "Lịch sử đóng/mở cửa ⬇️";
    }
});

/* Clear Access History */
clearHistoryBtn.onclick = () => {
    if (confirm("Bạn có chắc chắn muốn xóa toàn bộ lịch sử đóng/mở cửa?")) {
        db.ref("/SmartLock/smartlock_history").remove()
            .then(() => {
                historyList.innerHTML = "";
                alert("Lịch sử đóng/mở cửa đã được xóa.");
            })
            .catch((error) => {
                console.error("Lỗi khi xóa lịch sử: ", error);
                alert("Đã có lỗi xảy ra khi xóa lịch sử.");
            });
    }
};
