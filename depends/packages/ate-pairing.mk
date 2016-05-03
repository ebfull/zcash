package=ate-pairing
$(package)_version=0.1
$(package)_download_path=https://github.com/herumi/$(package)/archive/
$(package)_file_name=$(package)-$($(package)_git_commit).tar.gz
$(package)_download_file=$($(package)_git_commit).tar.gz
$(package)_sha256_hash=02e0a00a57eab70375b005b96df358eeadd7b86f59771c4fb42286e72dd17611
$(package)_dependencies=xbyak libgmp

$(package)_git_commit=40c7f5d8c67fc64a4baa71606c5869c115e6573f

define $(package)_build_cmds
  $(MAKE) SUPPORT_SNARK=1 INC_DIR='-fPIC -I../include $($(package)_cppflags)' LIB_DIR='-L../lib $($(package)_ldflags)'
endef

define $(package)_stage_cmds
  cp -rv include/ lib/ $($(package)_staging_dir)$(host_prefix)
endef
